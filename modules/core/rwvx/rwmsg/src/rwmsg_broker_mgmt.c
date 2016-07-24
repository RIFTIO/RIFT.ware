
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_broker_mgmt.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG broker management interface
 */

#include "rwmsg_int.h"
#include "rwmsg_broker.h"
#include "rwmsg-data.pb-c.h"
#include "rwmsg_broker_mgmt.h"

static void rwmsg_broker_mgmt_get_methbtab_free(int ct, RWPB_T_MSG(RwmsgData_Methbinding) *methbtab) {
  free(methbtab);
}
static rw_status_t rwmsg_broker_mgmt_get_methbtab_FREEME(rwmsg_broker_t *bro, 
							 rwmsg_channel_t *ch, 
							 int *ct_out, 
							 RWPB_T_MSG(RwmsgData_Methbinding) **methbtab_freeme) {
  RW_ASSERT(methbtab_freeme);
  RW_ASSERT(ct_out);

  int mct = 0;
  RWPB_T_MSG(RwmsgData_Methbinding) *tab = NULL;
  int tablen = 0;

  if (ch->chantype != RWMSG_CHAN_PEERSRV && ch->chantype != RWMSG_CHAN_BROSRV)
    goto ret;

  RWMSG_RG_LOCK();
  {
    struct rwmsg_methbinding_s *mb = NULL;
    while ((mb = (mb ? RW_DL_NEXT(mb, struct rwmsg_methbinding_s, elem) : RW_DL_HEAD(&((rwmsg_broker_srvchan_t*)ch)->methbindings, struct rwmsg_methbinding_s, elem)))) {
      uint32_t i;
      for (i=0; i<mb->srvchans_ct; i++) {
        if (mb->srvchans[i].ch == ch) {
          if (mct == tablen) {
            const int incr = 50;
            int newlen = tablen + incr;
            tab = realloc(tab, sizeof(tab[0]) * newlen);
            RW_ASSERT(tab);
            memset(&tab[tablen], 0, incr*sizeof(tab[0]));
            tablen = newlen;
          }

          /* This method points to us */
          RWPB_T_MSG(RwmsgData_Methbinding) *b = &tab[mct++];

          RWPB_F_MSG_INIT(RwmsgData_Methbinding,b);

          b->has_btype = TRUE;
          switch (mb->k.bindtype) {
          case RWMSG_METHB_TYPE_SRVCHAN:
            b->btype = RWMSG_METHB_TYPE_SRVCHAN;
            break;
          case RWMSG_METHB_TYPE_BROSRVCHAN:
            b->btype = RWMSG_METHB_TYPE_BROSRVCHAN;
            break;
          default:
            b->has_btype = FALSE;
            break;
          }

          b->pathhash = mb->k.pathhash;
          b->has_methno = TRUE;
          b->methno = mb->k.methno;

          b->has_payfmt = TRUE;
          switch (mb->k.payt) {
          case RWMSG_PAYFMT_RAW:
            b->payfmt = RWMSG_DATA_RWMSG_PAYLOAD_YENUM_RAW;
            break;
          case RWMSG_PAYFMT_PBAPI:
            b->payfmt = RWMSG_DATA_RWMSG_PAYLOAD_YENUM_PBAPI;
            b->pbapi_methno = RWMSG_PBAPI_SUBVAL_METHOD(b->methno);
            b->pbapi_srvno = RWMSG_PBAPI_SUBVAL_SERVICE(b->methno);
            break;
          case RWMSG_PAYFMT_TEXT:
            b->payfmt = RWMSG_DATA_RWMSG_PAYLOAD_YENUM_TEXT;
            break;
          case RWMSG_PAYFMT_MSGCTL:
            b->payfmt = RWMSG_DATA_RWMSG_PAYLOAD_YENUM_MSGCTL;
            break;
          default:
            b->has_payfmt = FALSE;
            break;
          }

          b->has_path = TRUE;
          strncpy(b->path, mb->meth->path, sizeof(b->path));
          b->path[sizeof(b->path)-1] = '\0';

          b->has_srvchanct = TRUE;
          b->srvchanct = mb->srvchans_ct;
        }
      }
      //ent = NULL;

      RW_ASSERT(mct <= tablen);
    }
  }
  RWMSG_RG_UNLOCK();

ret:
  *ct_out = mct;
  if (mct) {
    *methbtab_freeme = tab;
  } else {
    *methbtab_freeme = NULL;
    free(tab);
  }

  return RW_STATUS_SUCCESS;
}


struct get_socktab_s {
  rwmsg_broker_t *bro;
  rwmsg_channel_t *ch;
  int ct;
  RWPB_T_MSG(RwmsgData_Chansocket) *tab;
};

static void get_socktab(void *ud) {
  struct get_socktab_s *gst = (struct get_socktab_s *)ud;

  gst->ct = 0;
  gst->tab = NULL;
  int tablen = 0;

  if (gst->ch->ss) {
    int pri;
    for (pri=0; pri < gst->ch->ss->sk_ct; pri++) {
      if (!tablen) {
	RW_ASSERT(gst->ct == 0);
	tablen = gst->ch->ss->sk_ct;
	gst->tab = calloc(tablen, sizeof(gst->tab[0]));
      }

      RWPB_T_MSG(RwmsgData_Chansocket) *s = &gst->tab[gst->ct++];
      RWPB_F_MSG_INIT(RwmsgData_Chansocket,s);

      s->has_connected = TRUE;
      s->connected = !!(((1<<pri) & gst->ch->ss->sk_mask));

      if (s->connected && gst->ch->ss->sktype == RWMSG_SOCKSET_SKTYPE_NN) {

	struct rwmsg_sockset_nnsk_s *sk = &gst->ch->ss->nn.sk[pri];
	RW_ASSERT(sk);

	s->has_paused = TRUE;
	s->paused = sk->paused;
	
	//s->has_pri = TRUE;
	s->pri = pri;

        switch (sk->state) {
	case RWMSG_NNSK_IDLE:
	  s->has_state = TRUE;
	  s->state = RWMSG_DATA_RWMSG_SKSTATE_YENUM_NNSK_IDLE;
	  break;
	case RWMSG_NNSK_CONNECTED:
	  s->has_state = TRUE;
	  s->state = RWMSG_DATA_RWMSG_SKSTATE_YENUM_NNSK_CONNECTED;
	  break;
	default:
	  break;
	}
      }
    }
  }
}
static void rwmsg_broker_mgmt_get_socktab_free(int ct, RWPB_T_MSG(RwmsgData_Chansocket) *socktab) {
  free(socktab);
}
static void rwmsg_broker_mgmt_get_socktab_FREEME(rwmsg_broker_t *bro, rwmsg_channel_t *ch, int *ct_out, RWPB_T_MSG(RwmsgData_Chansocket) **socktab_freeme) {
  RW_ASSERT(socktab_freeme);
  RW_ASSERT(ct_out);

  struct get_socktab_s gst = { 0 };

  gst.bro = bro;
  gst.ch = ch;

  if (bro->use_mainq) {
    get_socktab(&gst);
  } else {
    // horrid upcast, gack
    rwmsg_broker_channel_t *bch = (rwmsg_broker_channel_t *)ch;
    rwsched_dispatch_sync_f(bro->ep->taskletinfo,
			    bch->rwq,
			    &gst,
			    get_socktab);
  }

  *ct_out = gst.ct;
  if (gst.ct) {
    *socktab_freeme = gst.tab;
  } else {
    *socktab_freeme = NULL;
    if (gst.tab) {
      free(gst.tab);
      gst.tab = NULL;
    }
  }

  return;
}


void rwmsg_broker_mgmt_get_chtab_free(int ct, RWPB_T_MSG(RwmsgData_Channelinfo) *chtab) {
  int i;
  for (i=0; i<ct; i++) {
    if (chtab[i].n_methbindings) {
      RW_ASSERT(chtab[i].methbindings);
      RW_ASSERT(chtab[i].methbindings[0]);
      rwmsg_broker_mgmt_get_methbtab_free(chtab[i].n_methbindings, chtab[i].methbindings[0]); /* original methtab */
      free(chtab[i].methbindings); /* pointer array */
      chtab[i].n_methbindings = 0;
      chtab[i].methbindings = NULL;
    }
    if (chtab[i].n_sockets) {
      RW_ASSERT(chtab[i].sockets);
      RW_ASSERT(chtab[i].sockets[0]);
      rwmsg_broker_mgmt_get_socktab_free(chtab[i].n_sockets, chtab[i].sockets[0]);
      free(chtab[i].sockets);
      chtab[i].n_sockets = 0;
      chtab[i].sockets = NULL;
    }
  }
  free(chtab);
}

rw_status_t rwmsg_broker_mgmt_get_chtab_FREEME(rwmsg_broker_t *bro, int *ct_out, RWPB_T_MSG(RwmsgData_Channelinfo) **chtab_freeme) {
  RW_ASSERT(chtab_freeme);
  RW_ASSERT(ct_out);

  /* We assume that we are running in the CFRunloop aka Main Queue
     context, as that is where the acceptor runs. */

  int chtab_ct = HASH_COUNT(bro->acc.bch_hash);
  RWPB_T_MSG(RwmsgData_Channelinfo) *chtab = calloc(chtab_ct, sizeof(RWPB_T_MSG(RwmsgData_Channelinfo)));
  *chtab_freeme = chtab;

  rwmsg_broker_channel_t *bch=NULL, *tmp=NULL;
  int ct = 0;
  HASH_ITER(hh, bro->acc.bch_hash, bch, tmp) {
    RW_ASSERT(ct < chtab_ct);

    RWPB_F_MSG_INIT(RwmsgData_Channelinfo,chtab);

    chtab->has_type = TRUE;
    switch (bch->ch.chantype) {
    case RWMSG_CHAN_BROCLI:
      chtab->type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_BROCLI;
      break;
    case RWMSG_CHAN_BROSRV:
      chtab->type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_BROSRV;
      chtab->has_clict = TRUE;
      {
        /* slightly sketchy direct examination of another thread's variable */
        rwmsg_broker_srvchan_t *bsc = (rwmsg_broker_srvchan_t *)bch;
        chtab->clict = HASH_COUNT(bsc->cli_hash);
      }
      break;
    case RWMSG_CHAN_PEERCLI:
      chtab->type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_PEERCLI;
      break;
    case RWMSG_CHAN_PEERSRV:
      chtab->type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_PEERSRV;
      chtab->has_clict = TRUE;
      {
        /* slightly sketchy direct examination of another thread's variable */
        rwmsg_broker_srvchan_t *bsc = (rwmsg_broker_srvchan_t *)bch;
        chtab->clict = HASH_COUNT(bsc->cli_hash);
      }
      break;
    case RWMSG_CHAN_SRV:
      chtab->type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_SRV;
      break;
    case RWMSG_CHAN_CLI:
      chtab->type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_CLI;
      break;
    default:
      chtab->type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_NULL;
      break;
    }

    /* Global ID of broker channel */
    //chtab->id = (uint64_t)bch->id.chanid;
    chtab->id = (uint64_t)bch->ch.chanid;

    /* Internal ID from client / server / peer */
    chtab->has_peer_id = TRUE;
    chtab->peer_id = bch->acc_key.chanid;

    /* Peer's IP address */
    if (bch->ch.chantype != RWMSG_CHAN_PEERSRV &&
        inet_ntop(AF_INET, &bch->acc_key.ipv4, chtab->peer_ip, sizeof(chtab->peer_ip))) {
      RW_ASSERT(bch->ch.chantype != RWMSG_CHAN_SRV);
      RW_ASSERT(bch->ch.chantype != RWMSG_CHAN_CLI);
      chtab->has_peer_ip = TRUE;
    }

    /* Peer's PID */
    chtab->has_peer_pid = TRUE;
    chtab->peer_pid = bch->acc_key.pid; // u32

    int methbct = 0;
    RWPB_T_MSG(RwmsgData_Methbinding) *methbtab = NULL;
    rwmsg_broker_mgmt_get_methbtab_FREEME(bro, &bch->ch, &methbct, &methbtab);
    if (methbct) {
      chtab->n_methbindings = methbct;
      chtab->methbindings = calloc(methbct, sizeof(methbtab));
      int j;
      for (j=0; j<methbct; j++) {
        chtab->methbindings[j] = &methbtab[j];
      }
    }

    /* Get the rest, via dispatch_sync to the peer's queue. */
    //...or via function call if all on mainq

    int sockct = 0;
    RWPB_T_MSG(RwmsgData_Chansocket) *socktab = NULL;
    rwmsg_broker_mgmt_get_socktab_FREEME(bro, &bch->ch, &sockct, &socktab);
    if (sockct) {
      chtab->n_sockets = sockct;
      chtab->sockets = calloc(sockct, sizeof(socktab));
      int j;
      for (j=0; j<sockct; j++) {
        chtab->sockets[j] = &socktab[j];
      }
    }

    chtab++;
    ct++;
  }
  RW_ASSERT(ct == chtab_ct);

  *ct_out = ct;
  return RW_STATUS_SUCCESS;
}

void rwmsg_broker_mgmt_get_chtab_debug_info_free(int ct, RWPB_T_MSG(RwmsgData_ChannelDebugInfo) *chtab) {
#if 1
  int i, j;
  for (i=0; i<ct; i++) {

    if (chtab[i].clichan_stats) {
      if (chtab[i].clichan_stats->n_bnc) {
        for (j=0; j<chtab[i].clichan_stats->n_bnc; j++) {
          free(chtab[i].clichan_stats->bnc[j]);
          chtab[i].clichan_stats->bnc[j] = NULL;
        }
        free(chtab[i].clichan_stats->bnc);
        chtab[i].clichan_stats->bnc = NULL;
        free(chtab[i].clichan_stats);
        chtab[i].clichan_stats = NULL;
      }
    }

    if (chtab[i].srvchan_stats) {
      if (chtab[i].srvchan_stats->n_bnc) {
        for (j=0; j<chtab[i].srvchan_stats->n_bnc; j++) {
          free(chtab[i].srvchan_stats->bnc[j]);
          chtab[i].srvchan_stats->bnc[j] = NULL;
        }
        free(chtab[i].srvchan_stats->bnc);
        chtab[i].srvchan_stats->bnc = NULL;
        free(chtab[i].srvchan_stats);
        chtab[i].srvchan_stats = NULL;
      }
    }
  }
#endif
  free(chtab);
}

rw_status_t rwmsg_broker_mgmt_get_chtab_debug_info_FREEME(rwmsg_broker_t *bro, int *ct_out, RWPB_T_MSG(RwmsgData_ChannelDebugInfo) **chtab_freeme) {
  RW_ASSERT(chtab_freeme);
  RW_ASSERT(ct_out);

  /* We assume that we are running in the CFRunloop aka Main Queue
     context, as that is where the acceptor runs. */

  int chtab_ct = HASH_COUNT(bro->acc.bch_hash);
  RWPB_T_MSG(RwmsgData_ChannelDebugInfo) *chtab = calloc(chtab_ct, sizeof(RWPB_T_MSG(RwmsgData_ChannelDebugInfo)));
  *chtab_freeme = chtab;

  rwmsg_broker_channel_t *bch=NULL, *tmp=NULL;
  int ct = 0;
  HASH_ITER(hh, bro->acc.bch_hash, bch, tmp) {
    RW_ASSERT(ct < chtab_ct);

    RWPB_F_MSG_INIT(RwmsgData_ChannelDebugInfo,chtab);

    chtab->has_chan_type = TRUE;
    switch (bch->ch.chantype) {
    case RWMSG_CHAN_BROCLI:
      chtab->chan_type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_BROCLI;
      break;
    case RWMSG_CHAN_BROSRV:
      chtab->chan_type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_BROSRV;
      break;
    case RWMSG_CHAN_PEERCLI:
      chtab->chan_type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_PEERCLI;
      break;
    case RWMSG_CHAN_PEERSRV:
      chtab->chan_type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_PEERSRV;
      break;
    case RWMSG_CHAN_SRV:
      chtab->chan_type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_SRV;
      break;
    case RWMSG_CHAN_CLI:
      chtab->chan_type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_CLI;
      break;
    default:
      chtab->chan_type = RWMSG_DATA_RWMSG_CHANNEL_YENUM_NULL;
      break;
    }


    const char *bncdesc[] = {
      "OK",
      "NODEST",
      "NOMETH",
      "NOPEER",
      "BROKERR",
      "TIMEOUT",
      "RESET",
      "TERM",
      NULL
    };
    uint32_t i;

    switch (bch->ch.chantype) {
    case RWMSG_CHAN_BROCLI:
    case RWMSG_CHAN_PEERCLI:
      chtab->clichan_stats = malloc(sizeof(*chtab->clichan_stats));
      RWPB_F_MSG_INIT(RwmsgData_ChannelDebugInfo_ClichanStats, chtab->clichan_stats);

      if ((chtab->clichan_stats->recv = ((rwmsg_broker_clichan_t*)bch)->stat.recv)) chtab->clichan_stats->has_recv = TRUE;
      if ((chtab->clichan_stats->cancel = ((rwmsg_broker_clichan_t*)bch)->stat.cancel)) chtab->clichan_stats->has_cancel = TRUE;
      if ((chtab->clichan_stats->ss_pollout = ((rwmsg_broker_clichan_t*)bch)->stat.ss_pollout)) chtab->clichan_stats->has_ss_pollout = TRUE;
      if ((chtab->clichan_stats->defer = ((rwmsg_broker_clichan_t*)bch)->stat.defer)) chtab->clichan_stats->has_defer = TRUE;
      if ((chtab->clichan_stats->to_cansent = ((rwmsg_broker_clichan_t*)bch)->stat.to_cansent)) chtab->clichan_stats->has_to_cansent = TRUE;
      if ((chtab->clichan_stats->to_canreleased = ((rwmsg_broker_clichan_t*)bch)->stat.to_canreleased)) chtab->clichan_stats->has_to_canreleased = TRUE;
      if ((chtab->clichan_stats->ack_sent = ((rwmsg_broker_clichan_t*)bch)->stat.ack_sent)) chtab->clichan_stats->has_ack_sent = TRUE;
      if ((chtab->clichan_stats->ack_piggy_sent = ((rwmsg_broker_clichan_t*)bch)->stat.ack_piggy_sent)) chtab->clichan_stats->has_ack_piggy_sent = TRUE;
      if ((chtab->clichan_stats->ack_miss = ((rwmsg_broker_clichan_t*)bch)->stat.ack_miss)) chtab->clichan_stats->has_ack_miss = TRUE;
      if ((chtab->clichan_stats->recv_noent = ((rwmsg_broker_clichan_t*)bch)->stat.recv_noent)) chtab->clichan_stats->has_recv_noent = TRUE;
      if ((chtab->clichan_stats->recv_dup = ((rwmsg_broker_clichan_t*)bch)->stat.recv_dup)) chtab->clichan_stats->has_recv_dup = TRUE;

      for (i=0; i<RWMSG_BOUNCE_CT; i++) {
        if (((rwmsg_broker_clichan_t*)bch)->stat.bnc[i]) {
          chtab->clichan_stats->n_bnc++;
          chtab->clichan_stats->bnc = realloc(chtab->clichan_stats->bnc, chtab->clichan_stats->n_bnc*sizeof(*chtab->clichan_stats->bnc));
          RW_ASSERT(chtab->clichan_stats->bnc);
          chtab->clichan_stats->bnc[chtab->clichan_stats->n_bnc-1] = malloc(sizeof(*chtab->clichan_stats->bnc[chtab->clichan_stats->n_bnc-1]));
          RWPB_F_MSG_INIT(RwmsgData_ChannelDebugInfo_ClichanStats_Bnc, chtab->clichan_stats->bnc[chtab->clichan_stats->n_bnc-1]);
          chtab->clichan_stats->bnc[chtab->clichan_stats->n_bnc-1]->bnc_type = (char*)bncdesc[i];
          chtab->clichan_stats->bnc[chtab->clichan_stats->n_bnc-1]->has_count = TRUE;
          chtab->clichan_stats->bnc[chtab->clichan_stats->n_bnc-1]->count = ((rwmsg_broker_clichan_t*)bch)->stat.bnc[i];
        }
      }
      break;
    case RWMSG_CHAN_BROSRV:
    case RWMSG_CHAN_PEERSRV:
      chtab->srvchan_stats = malloc(sizeof(*chtab->srvchan_stats));
      RWPB_F_MSG_INIT(RwmsgData_ChannelDebugInfo_SrvchanStats, chtab->srvchan_stats);

      if ((chtab->srvchan_stats->cancel = ((rwmsg_broker_srvchan_t*)bch)->stat.cancel)) chtab->srvchan_stats->has_cancel = TRUE;
      if ((chtab->srvchan_stats->cancel_unk = ((rwmsg_broker_srvchan_t*)bch)->stat.cancel_unk)) chtab->srvchan_stats->has_cancel_unk = TRUE;
      if ((chtab->srvchan_stats->pending_donada = ((rwmsg_broker_srvchan_t*)bch)->stat.pending_donada)) chtab->srvchan_stats->has_pending_donada = TRUE;
      if ((chtab->srvchan_stats->cached_resend = ((rwmsg_broker_srvchan_t*)bch)->stat.cached_resend)) chtab->srvchan_stats->has_cached_resend = TRUE;
      if ((chtab->srvchan_stats->seqzero_recvd = ((rwmsg_broker_srvchan_t*)bch)->stat.seqzero_recvd)) chtab->srvchan_stats->has_seqzero_recvd = TRUE;

      for (i=0; i<RWMSG_BOUNCE_CT; i++) {
        if (((rwmsg_broker_srvchan_t*)bch)->stat.bnc[i]) {
          chtab->srvchan_stats->n_bnc++;
          chtab->srvchan_stats->bnc = realloc(chtab->srvchan_stats->bnc, chtab->srvchan_stats->n_bnc*sizeof(*chtab->srvchan_stats->bnc));
          RW_ASSERT(chtab->srvchan_stats->bnc);
          chtab->srvchan_stats->bnc[chtab->srvchan_stats->n_bnc-1] = malloc(sizeof(*chtab->srvchan_stats->bnc[chtab->srvchan_stats->n_bnc-1]));
          RWPB_F_MSG_INIT(RwmsgData_ChannelDebugInfo_SrvchanStats_Bnc, chtab->srvchan_stats->bnc[chtab->srvchan_stats->n_bnc-1]);
          chtab->srvchan_stats->bnc[chtab->srvchan_stats->n_bnc-1]->bnc_type = (char*)bncdesc[i];
          chtab->srvchan_stats->bnc[chtab->srvchan_stats->n_bnc-1]->has_count = TRUE;
          chtab->srvchan_stats->bnc[chtab->srvchan_stats->n_bnc-1]->count = ((rwmsg_broker_srvchan_t*)bch)->stat.bnc[i];
        }
      }
      break;
    default:
      break;
    }
    /* Global ID of broker channel */
    chtab->chan_id = (uint64_t)bch->id.chanid;

    chtab++;
    ct++;
  }
  RW_ASSERT(ct == chtab_ct);

  *ct_out = ct;
  return RW_STATUS_SUCCESS;
}
