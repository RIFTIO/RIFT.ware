
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_broker_acceptor.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG broker TCP acceptor
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "rwmsg_int.h"
#include "rwmsg_broker.h"
#include "rw-msg-log.pb-c.h"
#include "rw-log.pb-c.h"
#include "rwlog.h"

struct rwmsg_broker_acceptor_negotiating_s {
  rwmsg_broker_t *bro;

  int canhan;
  int fail;

  struct sockaddr_in addr;
  int sk;
  rwsched_dispatch_source_t rwstimer;
  rwsched_dispatch_source_t rwssocket;
  rwsched_instance_ptr_t rwsched;
  rwsched_tasklet_ptr_t taskletinfo;

  rw_dl_element_t elem;
};
static void freeneg(void*ptr) {
  struct rwmsg_broker_acceptor_negotiating_s *neg = (struct rwmsg_broker_acceptor_negotiating_s *) ptr;
  if (neg->sk >= 0) {
    close(neg->sk);
    neg->sk = -1;
  }
  if (neg->fail) {
    rwmsg_broker_g.exitnow.neg_freed_err++;
  }
  rwmsg_broker_g.exitnow.neg_freed++;
  RW_FREE_TYPE(ptr, struct rwmsg_broker_acceptor_negotiating_s);
}
static void haltneg(struct rwmsg_broker_acceptor_negotiating_s *neg) {
  rwsched_dispatch_source_cancel(neg->taskletinfo, neg->rwstimer);
  rwsched_dispatch_source_cancel(neg->taskletinfo, neg->rwssocket);
  RW_ASSERT(neg->bro);
  rwmsg_garbage(&neg->bro->ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, neg->rwstimer, neg->bro->ep->rwsched, neg->taskletinfo);
  neg->rwstimer = NULL;
  rwmsg_garbage(&neg->bro->ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, neg->rwssocket, neg->bro->ep->rwsched, neg->taskletinfo);
  neg->rwssocket = NULL;
  rwmsg_garbage(&neg->bro->ep->gc, RWMSG_OBJTYPE_GENERIC, neg, freeneg, NULL);
}

void rwmsg_broker_acceptor_negotiating_timeout_event(void *ud) {
  struct rwmsg_broker_acceptor_negotiating_s *neg = (struct rwmsg_broker_acceptor_negotiating_s *)ud;
  RW_ASSERT_TYPE(neg, struct rwmsg_broker_acceptor_negotiating_s);

  if (neg->canhan) {
    return;
  }
  //...

  RWMSG_TRACE(neg->bro->ep, NOTICE, "broker acceptor timeout sk=%d", neg->sk);
  
  if (rw_dl_contains_element(&neg->bro->acc.negotiating, &neg->elem)) {
    RW_DL_REMOVE(&neg->bro->acc.negotiating, neg, elem);
  }
  int clearbro = FALSE;
  if (neg->fail) {
    clearbro = TRUE;
  }
  neg->fail = TRUE;
  neg->canhan = 2;
  //  rwsched_dispatch_source_cancel(neg->taskletinfo, neg->rwstimer);
  //  rwsched_dispatch_source_cancel(neg->taskletinfo, neg->rwssocket);
  haltneg(neg);
  if (clearbro) {
    neg->bro = NULL;
  }
  return;
}

static void rwmsg_broker_acceptor_update_peer(rwmsg_broker_channel_t *bch) {
  if (!bch->ch.ss) {
    RWMSG_TRACE_CHAN(&bch->ch, WARN, "SKIPPING SKIPPING SKIPPING SKIPPING SKIPPING - SOCKET NOT UP bch=%p", bch);
    RWMSG_LOG_EVENT(bch->ch.ep, MsgWarning, RWLOG_ATTR_SPRINTF("SKIPPING - MSG SOCKET NOT UP bch=%p\n", bch));
    return;
  }
  else {
    RWMSG_TRACE_CHAN(&bch->ch, INFO, "SENDING the methbinding to peer-broker that just CONNECTED bch=%p", bch);
  }
  RW_ASSERT(bch->bro);

  rwmsg_broker_t *bro = bch->bro;
  rwmsg_broker_channel_t *i_bch=NULL;
  rwmsg_broker_channel_t *tmp=NULL;
  int ct_lock=0, ct_unlock=0;

  /* We are in broker acceptor thread, so this hash is stable */
  HASH_ITER(hh, bro->acc.bch_hash, i_bch, tmp) {
    if (i_bch->ch.chantype != RWMSG_CHAN_PEERSRV && i_bch->ch.chantype != RWMSG_CHAN_BROSRV) {
      continue;
    }
    if (i_bch->bro->bro_instid != bch->bro->bro_instid) {
      continue;
    }

    /* Methbinding list is manipulated from arbitrary channel threads, we take the RG (global) lock */
    RWMSG_RG_LOCK();
    ct_lock++;
    struct rwmsg_methbinding_s *mb = NULL;
    for (mb = RW_DL_HEAD(&((rwmsg_broker_srvchan_t *)i_bch)->methbindings, struct rwmsg_methbinding_s, elem);
         mb;
         mb = RW_DL_NEXT(mb, struct rwmsg_methbinding_s, elem)) {
      RW_ASSERT(mb);
      RW_ASSERT(mb->meth);
      RW_ASSERT(mb->meth->sig);
      rw_status_t rs;
      if (mb->k.bindtype != RWMSG_METHB_TYPE_BROSRVCHAN) {
        continue;
      }
      if (mb->k.bro_instid != bch->bro->bro_instid) {
        continue;
      }
      if (!mb->local_service) {
        continue;
      }
      RW_ASSERT(bch->bro->bro_instid != 0);
      
      RWMSG_TRACE_CHAN(&bch->ch, INFO, "Sending the methbinding to peer-broker that just CONNECTED  path='%s' phash=0x%lx methno=%u payt=%d bro_instid=%d",
                       mb->meth->path,
                       mb->k.pathhash,
                       mb->k.methno,
                       mb->k.payt,
                       mb->k.bro_instid);

      /* It is a bit distressing to do this while holding the RG lock */
      rs = rwmsg_send_methbinding(&bch->ch, mb->meth->sig->pri, &mb->k, mb->meth->path);
      if (rs != RW_STATUS_SUCCESS) {
        rwmsg_sockset_pollout(bch->ch.ss, mb->meth->sig->pri, TRUE);
        RWMSG_TRACE_CHAN(&bch->ch, WARN, "SEND FAILED SEND FAILED bch=%p", bch);
        break; // to UNLOCK
      }
    }
    RWMSG_RG_UNLOCK();
    ct_unlock++;
  }

  RW_ASSERT(ct_lock == ct_unlock);
}


struct rwmsg_sockset_attach_block {
  rwmsg_sockset_t *ss;
  int fd;
  rwmsg_priority_t pri;
  rw_status_t rs;
};
static void rwmsg_broker_acceptor_sockset_attach_fd_f(void *ctx_in) {
  struct rwmsg_sockset_attach_block *att = (struct rwmsg_sockset_attach_block *)ctx_in;
  att->rs = rwmsg_sockset_attach_fd(att->ss, att->fd, att->pri);
}


void rwmsg_broker_acceptor_negotiating_socket_event(void *ud) {
  struct rwmsg_broker_acceptor_negotiating_s *neg = (struct rwmsg_broker_acceptor_negotiating_s *)ud;
  RW_ASSERT_TYPE(neg, struct rwmsg_broker_acceptor_negotiating_s);

  rwmsg_broker_acceptor_t *acc = &neg->bro->acc;

  if (neg->canhan) {
    return;
  }
  RW_ASSERT(neg->bro);

  struct rwmsg_handshake_s handshake;
  
  int r;
  r = recv(neg->sk, &handshake, sizeof(handshake), MSG_DONTWAIT);
  if (r < 0) {
    switch (errno) {
    case EAGAIN:
    case EINTR:
      goto ret;
    default:
      {
	int errtmp = errno;
	RWMSG_TRACE(neg->bro->ep, NOTICE, "broker acceptor dud handshake read, errno=%d '%s'", errtmp, strerror(errtmp));
      }
      goto bail;
    }
  } else if (r != sizeof(handshake)) {
    RWMSG_TRACE(neg->bro->ep, NOTICE, "broker acceptor dud handshake read, size=%d", r);
    goto bail;
  }

  RWMSG_TRACE(neg->bro->ep, INFO, "handshake .chanid=%u .pid=%u .pri=%u .chtype=%u .instid=%u",
         handshake.chanid,
         handshake.pid,
         handshake.pri,
         handshake.chtype,
         handshake.instid);

  struct rwmsg_broker_channel_acceptor_key_s key;
  RW_ZERO_VARIABLE(&key);
  key.chanid = handshake.chanid;
  key.pid = handshake.pid;
  key.ipv4 = neg->addr.sin_addr.s_addr;
  key.instid = handshake.instid;

  rwmsg_broker_channel_t *bch = NULL;
  HASH_FIND(hh, acc->bch_hash, &key, sizeof(key), bch);
  if (!bch) {
    /* Create */
    char *chdesc = "??";
    switch(handshake.chtype) {
    case RWMSG_CHAN_CLI:
      bch = (rwmsg_broker_channel_t *)rwmsg_broker_clichan_create(neg->bro, &key);
      chdesc = "bclichan";
      break;
    case RWMSG_CHAN_SRV:
      bch = (rwmsg_broker_channel_t *)rwmsg_broker_srvchan_create(neg->bro, &key);
      chdesc = "bsrvchan";
      break;
    case RWMSG_CHAN_PEERSRV:
      bch = (rwmsg_broker_channel_t *)rwmsg_peer_clichan_create(neg->bro, &key);
      chdesc = "psrvchan";
      break;
    default:
      RWMSG_TRACE(neg->bro->ep, NOTICE, "broker acceptor dud handshake chtype of %d", handshake.chtype);
      goto bail;
      break;
    }
    RW_ASSERT(bch->rwq);
    rwmsg_channel_bind_rwsched_queue(&bch->ch, bch->rwq);

    RWMSG_TRACE(neg->bro->ep, INFO, "broker acceptor new broker channel chdesc='%s' .chanid=%u .pid=%u ip=%s", 
	    chdesc,
	    handshake.chanid,
	    handshake.pid,
	    inet_ntoa(neg->addr.sin_addr));

    /* Now it must exist */
    rwmsg_broker_channel_t *tmpbch;
    HASH_FIND(hh, acc->bch_hash, &key, sizeof(key), tmpbch);
    RW_ASSERT(tmpbch == bch);
  }

  /* Add this socket to the channel */
  rw_status_t rs;
  bch->ch.tickle = TRUE;
  if (bch->bro->use_mainq) {
    /* All one queue, just do it */
    rs = rwmsg_sockset_attach_fd(bch->ch.ss, neg->sk, handshake.pri);
  } else {
    /* Do so synchronously in the channel's serial queue.  Gah!  FIXME to be async. */
    struct rwmsg_sockset_attach_block att = { };
    att.ss = bch->ch.ss;
    att.fd = neg->sk;
    att.pri = handshake.pri;
    att.rs = RW_STATUS_FAILURE;
    rwsched_dispatch_sync_f(bch->bro->ep->taskletinfo,
			    bch->rwq,
			    &att,
			    rwmsg_broker_acceptor_sockset_attach_fd_f);
    rs = att.rs;
  }
  if (rs != RW_STATUS_SUCCESS) {
    RWMSG_TRACE(bch->ch.ep, ERROR, "broker acceptor sockset_attach_fd failure rs=%d fd=%d\n", rs, neg->sk);
    goto bail;
  }

  rwmsg_broker_g.exitnow.neg_accepted++;
  
  RWMSG_TRACE(bch->ch.ep, NOTICE,
	      "broker-instance [%d] broker_acceptor_negotiating_socket_event accepted and attached fd=%d to bch=%p from handshake"
	      " chanid=%u chtyp=%u(%s) pid=%u pri=%u from addr %s port %u", 
	      bch->bro->bro_instid,
	      neg->sk,
	      bch,
	      handshake.chanid, 
	      handshake.chtype, 
	      (handshake.chtype == RWMSG_CHAN_CLI ? "clichan" 
	       :handshake.chtype == RWMSG_CHAN_SRV ? "srvchan"
	       :handshake.chtype == RWMSG_CHAN_PEERSRV ? "peersrvchan"
	       : "dud!"),
	      handshake.pid, handshake.pri,
	      inet_ntoa(neg->addr.sin_addr),
	      (uint32_t)ntohs(neg->addr.sin_port));

  // leave open in async dtor
  neg->sk = -1;

  if (handshake.chtype == RWMSG_CHAN_PEERSRV && handshake.pri == RWMSG_PRIORITY_PLATFORM) {
    rwmsg_broker_acceptor_update_peer(bch);
  }

 cancel:
  neg->canhan = 2;
  RW_DL_REMOVE(&neg->bro->acc.negotiating, neg, elem);
  haltneg(neg);
  //  rwsched_dispatch_source_cancel(bch->bro->ep->taskletinfo, neg->rwstimer);
  //  rwsched_dispatch_source_cancel(bch->bro->ep->taskletinfo, neg->rwssocket);
  neg->bro = NULL;
 ret:
  return;

 bail:
  neg->fail = TRUE;
  goto cancel;
}

void rwmsg_broker_acceptor_event(void *ud, int bits) {
  rwmsg_broker_t *bro = (rwmsg_broker_t *)ud;
  RW_ASSERT_TYPE(bro, rwmsg_broker_t);

  rwmsg_broker_acceptor_t *acc = &bro->acc;

  struct sockaddr ad;
  unsigned int adlen = sizeof(ad);
  int nsk = accept(acc->sk, &ad, &adlen);
  if (nsk >= 0) {
    RW_ASSERT(nsk != 0);	/* who closed stdin? */

    int fl;
    fl = fcntl(nsk, F_GETFD);
    if (fl < 0) {
      perror("broker acceptor fcntl F_GETFD");
      goto err;
    }
    fl |= FD_CLOEXEC;
    int r = fcntl(nsk, F_SETFD, fl);
    if (r < 0) {
      perror("broker acceptor fcntl F_SETFD");
      goto err;
    }
    int one=1;
    r = setsockopt(nsk, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (r < 0) {
      perror("broker acceptor TCP_NODELAY");
      goto err;
    }

    if (1) {
      int soval=0;
      socklen_t soval_sz=sizeof(soval);
      r = getsockopt(acc->sk, SOL_SOCKET, SO_SNDBUF, &soval, &soval_sz);
      RW_ASSERT(r == 0);
      RW_ASSERT(soval_sz == sizeof(int));
      int sndbuf = soval;
      
      soval=0;
      soval_sz=sizeof(soval);
      r = getsockopt(acc->sk, SOL_SOCKET, SO_RCVBUF, &soval, &soval_sz);
      RW_ASSERT(r == 0);
      RW_ASSERT(soval_sz == sizeof(int));

      static int once;
      if (!once++) {
	RWMSG_TRACE(bro->ep, INFO, "broker acceptor socket effective SO_SNDBUF %d SO_RCVBUF %d\n", sndbuf, soval);
      }
    }


    struct rwmsg_broker_acceptor_negotiating_s *neg;
    neg = (struct rwmsg_broker_acceptor_negotiating_s *)RW_MALLOC0_TYPE(sizeof(*neg), struct rwmsg_broker_acceptor_negotiating_s);

    RWMSG_TRACE(bro->ep, DEBUG, "broker acceptor event nsk=%d neg=%p", nsk, neg);

    memcpy(&neg->addr, &ad, sizeof(neg->addr));
    neg->bro = bro;
    neg->rwsched = bro->ep->rwsched;
    neg->taskletinfo = bro->ep->taskletinfo;
    neg->sk = nsk;

    rwsched_dispatch_queue_t q = rwsched_dispatch_get_main_queue(bro->ep->rwsched);

    neg->rwssocket = rwsched_dispatch_source_create(bro->ep->taskletinfo,
                                                    RWSCHED_DISPATCH_SOURCE_TYPE_READ,
                                                    nsk,
                                                    0,
						    q);
    rwsched_dispatch_set_context(bro->ep->taskletinfo,
                                 neg->rwssocket,
                                 neg);
    /*    rwsched_dispatch_source_set_cancel_handler_f(bro->ep->taskletinfo,
						 neg->rwssocket, 
						 rwmsg_broker_acceptor_negotiating_cancel_event);
    */
    rwsched_dispatch_source_set_event_handler_f(bro->ep->taskletinfo,
                                                neg->rwssocket,
                                                rwmsg_broker_acceptor_negotiating_socket_event);
    

    neg->rwstimer = rwsched_dispatch_source_create(bro->ep->taskletinfo,
                                                   RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                   0,
                                                   0,
                                                   q);
    /*    rwsched_dispatch_source_set_cancel_handler_f(bro->ep->taskletinfo,
						 neg->rwstimer,
						 rwmsg_broker_acceptor_negotiating_cancel_event);
    */
    rwsched_dispatch_source_set_event_handler_f(bro->ep->taskletinfo,
                                                neg->rwstimer, 
                                                rwmsg_broker_acceptor_negotiating_timeout_event);
    rwsched_dispatch_set_context(bro->ep->taskletinfo,
                                 neg->rwstimer, 
                                 neg);
    rwsched_dispatch_source_set_timer(bro->ep->taskletinfo,
                                      neg->rwstimer,
                                      dispatch_time(DISPATCH_TIME_NOW, 1*NSEC_PER_SEC),
                                      0,
                                      0);

    RW_DL_INSERT_HEAD(&acc->negotiating, neg, elem);

    rwsched_dispatch_resume(bro->ep->taskletinfo, neg->rwssocket);
    rwsched_dispatch_resume(bro->ep->taskletinfo, neg->rwstimer);

  }

  return;

 err:
  close(nsk);
  return;

  bits=bits;
}


static void rwmsg_broker_acceptor_deinit_finish(rwmsg_broker_t *bro) {

  rwmsg_broker_acceptor_t *acc = &bro->acc;
  
  if (!acc->notify.cancelack) {
    RWMSG_TRACE(bro->ep, INFO, "broker acceptor deinit finish, notify=%p cancelack still unset", &acc->notify);
    rwmsg_garbage(&bro->ep->gc, RWMSG_OBJTYPE_GENERIC, bro, (void*)&rwmsg_broker_acceptor_deinit_finish, NULL);
    return;
  }


  close(acc->sk);

  struct rwmsg_broker_acceptor_negotiating_s *neg;
  do {
    neg = RW_DL_DEQUEUE(&acc->negotiating, struct rwmsg_broker_acceptor_negotiating_s, elem);
    if (neg) {
      neg->fail = TRUE;
      rwmsg_broker_acceptor_negotiating_timeout_event(neg);
    }
  } while(neg);

  rwmsg_broker_channel_t *bch, *tmp;
  HASH_ITER(hh, acc->bch_hash, bch, tmp) {
    switch(bch->ch.chantype) {
    case RWMSG_CHAN_BROSRV:
      rwmsg_broker_srvchan_halt((rwmsg_broker_srvchan_t*)bch);
      break;
    case RWMSG_CHAN_BROCLI:
      rwmsg_broker_clichan_halt((rwmsg_broker_clichan_t*)bch);
      break;
    case RWMSG_CHAN_PEERSRV:
      rwmsg_broker_srvchan_halt((rwmsg_broker_srvchan_t*)bch);
      break;
    case RWMSG_CHAN_PEERCLI:
      rwmsg_broker_clichan_halt((rwmsg_broker_clichan_t*)bch);
      break;
    default:
      RW_CRASH();
      break;
    }
    HASH_DELETE(hh, acc->bch_hash, bch);
  }
  return;
}

void rwmsg_broker_acceptor_halt_bch_and_remove_from_hash_f(void *ctx) {
  rwmsg_broker_channel_t *bch = (rwmsg_broker_channel_t*)ctx;
  rwmsg_broker_t *bro = bch->bro;
  rwmsg_broker_acceptor_t *acc = &bro->acc;

  switch(bch->ch.chantype) {
  case RWMSG_CHAN_BROSRV:
  case RWMSG_CHAN_PEERSRV:
    rwmsg_broker_srvchan_release((rwmsg_broker_srvchan_t*)bch);
    rwmsg_broker_srvchan_halt((rwmsg_broker_srvchan_t*)bch);
    break;
  case RWMSG_CHAN_BROCLI:
  case RWMSG_CHAN_PEERCLI:
    rwmsg_broker_clichan_release((rwmsg_broker_clichan_t*)bch);
    rwmsg_broker_clichan_halt((rwmsg_broker_clichan_t*)bch);
    break;
  default:
    RW_CRASH();
    break;
  }

  if (acc->bch_hash)
    HASH_DELETE(hh, acc->bch_hash, bch);

  return;
}

void rwmsg_broker_acceptor_deinit(rwmsg_broker_acceptor_t *acc,
				  rwmsg_broker_t *bro) {

  if (!acc->notify.halted) {

    RWMSG_TRACE(bro->ep, INFO, "broker acceptor deinit, setting notify.wantack in notify=%p", &acc->notify);

    acc->notify.wantack = TRUE;
    rwmsg_notify_deinit(bro->ep, &acc->notify);
    rwmsg_garbage(&bro->ep->gc, RWMSG_OBJTYPE_GENERIC, bro, (void*)&rwmsg_broker_acceptor_deinit_finish, NULL);
  }
}

rw_status_t rwmsg_broker_acceptor_init(rwmsg_broker_acceptor_t *acc,
				       rwmsg_broker_t *bro,
				       uint32_t ipaddr,
				       uint16_t port) {
  
  rw_status_t rs = RW_STATUS_FAILURE;

  memset(acc, 0, sizeof(*acc));

  acc->sk = socket(AF_INET, SOCK_STREAM, 0);
  if (acc->sk < 0) {
    goto err;
  }

  int r;
  int soval = 1;
  r = setsockopt(acc->sk, SOL_SOCKET, SO_REUSEADDR, &soval, sizeof(soval));
  if (r < 0) {
    perror("broker acceptor setsockopt(SO_REUSEADDR)");
    goto err;
  }
  soval = RWMSG_SO_SNDBUF;
  r = setsockopt(acc->sk, SOL_SOCKET, SO_SNDBUF, &soval, sizeof(soval));
  if (r < 0) {
    perror("broker acceptor setsockopt(SO_SNDBUF)");
    goto err;
  }
  soval = RWMSG_SO_RCVBUF;
  r = setsockopt(acc->sk, SOL_SOCKET, SO_RCVBUF, &soval, sizeof(soval));
  if (r < 0) {
    perror("broker acceptor setsockopt(SO_SNDBUF)");
    goto err;
  }

  int fl;
  fl = fcntl(acc->sk, F_GETFD);
  if (fl < 0) {
    perror("broker acceptor fcntl F_GETFD");
    goto err;
  }
  fl |= FD_CLOEXEC;
  r = fcntl(acc->sk, F_SETFD, fl);
  if (r < 0) {
    perror("broker acceptor fcntl F_SETFD");
    goto err;
  }
  fl = fcntl(acc->sk, F_GETFL);
  if (fl == -1) {
    perror("broker acceptor fcntl F_GETFL");
    goto err;
  }
  fl |= O_NONBLOCK;
  r = fcntl(acc->sk, F_SETFL, fl);
  if (r < 0) {
    perror("broker acceptor fcntl F_SETFL");
    goto err;
  }

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_port = htons(port);
  ad.sin_addr.s_addr = htonl(ipaddr);
  r = bind(acc->sk, (struct sockaddr*) &ad, sizeof(ad));
  if (r < 0) {
    perror("broker acceptor bind()");
    goto err;
  }

  r = listen(acc->sk, RWMSG_LISTEN_Q);
  if (r < 0) {
    perror("broker acceptor listen()");
    goto err;
  }

  acc->notify.theme = RWMSG_NOTIFY_RWSCHED_FD;
  acc->notify.rwsched.rwqueue = rwsched_dispatch_get_main_queue(bro->ep->rwsched);
  acc->notify.fd = acc->sk;
  acc->notify.order = 0;	/* highest priority is 0, although rwq isn't a priority q... */
  rwmsg_closure_t cb = { .notify = rwmsg_broker_acceptor_event, .ud = bro };
  rs = rwmsg_notify_init(bro->ep, &acc->notify, "acc->notify", &cb);
  if (rs != RW_STATUS_SUCCESS) {
    goto err;
  }

  if (rs == RW_STATUS_SUCCESS) {
    rwmsg_broker_g.exitnow.acc_listening++;
  }
  RWMSG_TRACE(bro->ep, INFO, "broker acceptor init rs=%d", rs);
  return rs;

 err:
  RWMSG_TRACE(bro->ep, CRIT, "broker acceptor init rs=%d", rs);
  if (acc->sk >= 0) {
    close(acc->sk);
    acc->sk = -1;
  }
  return rs;
}



