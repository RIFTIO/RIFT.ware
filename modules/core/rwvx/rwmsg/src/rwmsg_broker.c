
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */


/**
 * @file rwmsg_broker.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG broker core
 */

#include <sys/resource.h>
#include "rwmsg_int.h"
#include "rwmsg_broker.h"
#include "rwvcs_rwzk.h"
#include "rwtasklet.h"


/* Call in mainq thread, in a mainq callback or not */
rwmsg_bool_t rwmsg_broker_halt(rwmsg_broker_t *bro) {
  bro->halted = TRUE;
  if (bro == rwmsg_broker_g.abroker) {
    rwmsg_broker_g.abroker = NULL;
  }
  rwmsg_broker_acceptor_deinit(&bro->acc, bro);
  return TRUE;
}

/* Call outside the context of a mainq callback! */
rwmsg_bool_t rwmsg_broker_destroy(rwmsg_broker_t *bro) {
  bool zero = 0;
  ck_pr_dec_32_zero(&bro->refct, &zero);
  if (!zero) {
    return FALSE;
  }

  ck_pr_dec_32_zero(&rwmsg_broker_g.count, &zero);

  rwmsg_garbage_truck_flush(&bro->ep->gc, TRUE);
  rwmsg_bool_t rb = rwmsg_endpoint_halt_flush(bro->ep, TRUE);
  /* Endpoint may have been provided by the tasklet, so it may or may not go away here */
  if (bro->rwvcs_zk) {
    rw_status_t rs;
    char rwzk_path[999];
    char rwzk_LOCK[999];
    rwtasklet_info_t *ti = bro->rwtasklet_info;

    if (ti) {
      RW_ASSERT(bro->closure);
      sprintf(rwzk_path, "/sys-%u/rwmsg/broker/inst-%u", bro->sysid, bro->bro_instid);
      rs = rwvcs_rwzk_watcher_stop(ti->rwvcs, rwzk_path, &bro->closure);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);

      rs = rwvcs_rwzk_delete_path(ti->rwvcs, rwzk_path);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);

      if (zero) { /* all the brokers are gone */
        sprintf(rwzk_LOCK, "/sys/rwmsg/broker-lock/lock-%u", bro->sysid);
        rs = rwvcs_rwzk_delete_path(ti->rwvcs, rwzk_LOCK);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
      }
    }
    bro->rwvcs_zk = NULL;
  }

  memset(bro, 0, sizeof(*bro));
  RW_FREE_TYPE(bro, rwmsg_broker_t);
  return rb;
}

/* Call outside mainq context! */
rwmsg_bool_t rwmsg_broker_halt_sync(rwmsg_broker_t *bro) {
  rwmsg_broker_halt(bro);
  return rwmsg_broker_destroy(bro);
}
rwmsg_bool_t rwmsg_broker_test_halt (void *bro) {
  return rwmsg_broker_halt_sync ((rwmsg_broker_t *)bro);
}

static struct rwmsg_channel_funcs brofuncs[] = {
  { RWMSG_CHAN_BROSRV,
    (void(*)(rwmsg_channel_t*))rwmsg_broker_srvchan_halt_async,
    (void(*)(rwmsg_channel_t*))rwmsg_broker_srvchan_release,
    (uint32_t (*)(rwmsg_channel_t *ch)) rwmsg_broker_srvchan_recv, 
    (rw_status_t (*)(rwmsg_channel_t *cc, rwmsg_buf_t *buf)) rwmsg_broker_srvchan_recv_buf,
    rwmsg_broker_srvchan_sockset_event_send,
  },
  { RWMSG_CHAN_BROCLI,
    (void(*)(rwmsg_channel_t*))rwmsg_broker_clichan_halt_async,
    (void(*)(rwmsg_channel_t*))rwmsg_broker_clichan_release,
    (uint32_t (*)(rwmsg_channel_t *ch)) rwmsg_broker_clichan_recv, 
    (rw_status_t (*)(rwmsg_channel_t *cc, rwmsg_buf_t *buf)) rwmsg_broker_clichan_recv_buf,
    rwmsg_broker_clichan_sockset_event_send,
  },
  { RWMSG_CHAN_PEERSRV,
    (void(*)(rwmsg_channel_t*))rwmsg_broker_srvchan_halt_async,
    (void(*)(rwmsg_channel_t*))rwmsg_peer_srvchan_release,
    (uint32_t (*)(rwmsg_channel_t *ch)) rwmsg_peer_srvchan_recv, 
    (rw_status_t (*)(rwmsg_channel_t *cc, rwmsg_buf_t *buf)) rwmsg_peer_srvchan_recv_buf,
    rwmsg_peer_srvchan_sockset_event_send,
  },
  { RWMSG_CHAN_PEERCLI,
    (void(*)(rwmsg_channel_t*))rwmsg_broker_clichan_halt_async,
    (void(*)(rwmsg_channel_t*))rwmsg_peer_clichan_release,
    (uint32_t (*)(rwmsg_channel_t *ch)) rwmsg_peer_clichan_recv, 
    (rw_status_t (*)(rwmsg_channel_t *cc, rwmsg_buf_t *buf)) rwmsg_peer_clichan_recv_buf,
    rwmsg_broker_clichan_sockset_event_send,
  },
  //TBD bropeer
};

struct rwmsg_broker_global_s rwmsg_broker_g = { };


void rwmsg_broker_dump(rwmsg_broker_t *bro) {
  if (!bro) {
    bro = rwmsg_broker_g.abroker;
  } 
  if (!bro) {
    fprintf(stderr, "No broker in this process?!\n");
    return;
  }
  rwmsg_broker_channel_t *bch=NULL, *tmp=NULL;
  HASH_ITER(hh, bro->acc.bch_hash, bch, tmp) {
    fprintf(stderr, 
	    "%s channel %p typ %d chanid=%lu ",
      bch->ch.rwtpfx,
	    bch, 
	    bch->ch.chantype, 
	    (uint64_t)bch->ch.chanid);
    int nobro = FALSE;
    switch (bch->ch.chantype) {
    case RWMSG_CHAN_BROCLI:
      fprintf(stderr, "brocli ");
      break;
    case RWMSG_CHAN_BROSRV:
      fprintf(stderr, "brosrv ");
      break;
    case RWMSG_CHAN_PEERCLI:
      fprintf(stderr, "peercli ");
      break;
    case RWMSG_CHAN_PEERSRV:
      fprintf(stderr, "peersrv ");
      break;
    default:
      nobro=TRUE;
      fprintf(stderr, "other?!");
      break;
    }

    if (!nobro) {
      uint32_t reqct=0;
      if (bch->ch.chantype == RWMSG_CHAN_BROCLI ||
          bch->ch.chantype == RWMSG_CHAN_PEERCLI) {
        reqct = HASH_COUNT(((rwmsg_broker_clichan_t*)bch)->reqent_hash);
        fprintf(stderr, "recv-count=%lu ", ((rwmsg_broker_clichan_t*)bch)->stat.recv);
      } else {
        reqct = HASH_COUNT(((rwmsg_broker_srvchan_t*)bch)->req_hash);
      }
      fprintf(stderr, "reqs-count=%u", reqct);
    }
    fprintf(stderr, "\n");

    fprintf(stderr, 
	    "  localq=%p notify=%p(%u) qlen=%u/%u qsz=%u/%u full=%u\n"
	    "  ssbufq=%p notify=%p(%u) qlen=%u/%u qsz=%u/%u full=%u\n"
	    "  ss=%p sched.theme=%u sk_ct=%u closed=%u\n",

	    &bch->ch.localq,
	    bch->ch.localq.notify,
	    bch->ch.localq.notify ? bch->ch.localq.notify->theme : 0,
	    bch->ch.localq.qlen,
	    bch->ch.localq.qlencap,
	    bch->ch.localq.qsz,
	    bch->ch.localq.qszcap,
	    bch->ch.localq.full,

	    &bch->ch.ssbufq,
	    bch->ch.ssbufq.notify,
	    bch->ch.ssbufq.notify ? bch->ch.ssbufq.notify->theme : 0,
	    bch->ch.ssbufq.qlen,
	    bch->ch.ssbufq.qlencap,
	    bch->ch.ssbufq.qsz,
	    bch->ch.ssbufq.qszcap,
	    bch->ch.ssbufq.full,

	    bch->ch.ss,
	    bch->ch.ss->sched.theme,
	    bch->ch.ss->sk_ct,
	    bch->ch.ss->closed

	    );

    if (bch->ch.ss) {
      int pri;
      for (pri=0; pri < bch->ch.ss->sk_ct; pri++) {
	int connected = (((1<<pri) & bch->ch.ss->sk_mask));
	fprintf(stderr, "    pri=%u connected=%u\n",
		pri, 
		!!connected
		);

	struct rwmsg_sockset_nnsk_s *sk = &bch->ch.ss->nn.sk[pri];

	if (connected) {
	  switch (bch->ch.ss->sktype) {
	  case RWMSG_SOCKSET_SKTYPE_NN:
	    fprintf(stderr, "          sktype=NN sk=%p state=%s paused=%u notein.theme=%u noteout.theme=%u",
		    sk,
		    sk->state == RWMSG_NNSK_IDLE ? "IDLE" 
		    : sk->state == RWMSG_NNSK_CONNECTED ? "CONNECTED"
		    : "???",
		    sk->paused,
		    sk->notein.theme,
		    sk->noteout.theme);
	    break;
	  default:
	    fprintf(stderr, "          sktype=%u (!?)", bch->ch.ss->sktype);
	    break;
	  }
	}
	fprintf(stderr, "\n");
      }
    }
    RWMSG_RG_LOCK();
    {
      ck_ht_iterator_t iter = CK_HT_ITERATOR_INITIALIZER;
      ck_ht_entry_t *ent=NULL;
      while (ck_ht_next(&rwmsg_global.localdesttab, &iter, &ent)) {
        RW_ASSERT(ent);
        struct rwmsg_methbinding_s *mb = (struct rwmsg_methbinding_s *)ent->value;
        uint32_t i;
        for (i=0; i<mb->srvchans_ct; i++) {
          if (mb->srvchans[i].ch == &bch->ch) {
            fprintf(stderr, "  %u methbinding=%p k.bindtype=%u k.bro_instid=%u k.methno=%u phash=0x%lx path=%s\n",
                    mb->stat.found_called,
                    mb,
                    mb->k.bindtype, mb->k.bro_instid, mb->k.methno, mb->k.pathhash,
                    (mb->meth ? mb->meth->path : "NULL"));
          }
        }
      }
    }
    RWMSG_RG_UNLOCK();
  }
}

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/syscall.h>

#include <unistd.h>
#include <sys/syscall.h>

static rw_status_t rwmsg_broker_connect_to_peer(rwmsg_broker_t *bro, char *rwzk_get_data) {
  RW_ASSERT(bro);
  RW_ASSERT(rwzk_get_data);

  char *pnum = strrchr(rwzk_get_data, ':');
  RW_ASSERT(pnum);
  pnum++;
  RW_ASSERT(pnum);
  int port = atoi(pnum);

  struct rwmsg_broker_channel_acceptor_key_s key;
  RW_ZERO_VARIABLE(&key);
  key.chanid = port;
  key.pid = port;
  key.ipv4 = port;

  // hash-lookup, and connect only if bch is not found.
  rwmsg_broker_channel_t *bch = NULL;
  HASH_FIND(hh, bro->acc.bch_hash, &key, sizeof(key), bch);
  if (!bch) {
    /* FIXME: /rwmsg/broker-peer/nnuri property is currently used like a
     * global-variable that gets overwritten when a new broker-peer is connected
     * from this broker's endpoint.
     * rwmsg_channel_create() reads the property and connects to the peer.
     */
    rwmsg_endpoint_set_property_string(bro->ep, "/rwmsg/broker-peer/nnuri", rwzk_get_data);
    RWMSG_TRACE(bro->ep, INFO, "rwmsg_broker_connect_to_peer FROM** bro:%u ** TO** %s ** port=%d\n"
		                           "/rwmsg/broker-peer/nnuri = %s", bro->bro_instid, rwzk_get_data, port, rwzk_get_data);

    bch = (rwmsg_broker_channel_t *)rwmsg_peer_srvchan_create(bro, &key);
    RW_ASSERT(bch);
    RW_ASSERT(bch->rwq);
    rwmsg_channel_bind_rwsched_queue(&bch->ch, bch->rwq);

    /* Now it must exist */
    rwmsg_broker_channel_t *tmpbch;
    HASH_FIND(hh, bro->acc.bch_hash, &key, sizeof(key), tmpbch);
    RW_ASSERT(tmpbch == bch);
  }
  return RW_STATUS_SUCCESS;
}

static void rwmsg_broker_data_watcher_f(void *ctx) {
  rwmsg_broker_t *bro = (rwmsg_broker_t *)ctx;
  rw_status_t rs = RW_STATUS_FAILURE;
  char *rwzk_get_data = NULL;
  char ** children = NULL;;
  bro->data_watcher_cbct++;
  char rwzk_path[999];
  char rwzk_LOCK[999];
  char my_rwzk_path[999];
  struct timeval tv = { .tv_sec = 120, .tv_usec = 1000 };
  rwvcs_zk_module_ptr_t rwvcs_zk = bro->rwvcs_zk;
  if (rwvcs_zk==NULL) {
    return;
  }
  if (bro->halted) {
    return;
  }
  rwtasklet_info_t *ti = bro->rwtasklet_info;

  sprintf(rwzk_LOCK, "/sys/rwmsg/broker-lock/lock-%u", bro->sysid);
  RW_ASSERT(rwvcs_rwzk_exists(ti->rwvcs, rwzk_LOCK));
  rs = rwvcs_rwzk_lock_path(ti->rwvcs, rwzk_LOCK, &tv);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  sprintf(my_rwzk_path, "/sys-%u/rwmsg/broker/inst-%u", bro->sysid, bro->bro_instid);
  sprintf(rwzk_path, "/sys-%u/rwmsg/broker", bro->sysid);
  rs = rwvcs_rwzk_get_children(ti->rwvcs, rwzk_path, &children);
  RW_ASSERT(rs == RW_STATUS_SUCCESS || rs == RW_STATUS_NOTFOUND);
  if (children) {
    int i=0;
    while (children[i] != NULL) {
      sprintf(rwzk_path, "/sys-%u/rwmsg/broker/%s", bro->sysid, children[i]);
      if (strcmp(rwzk_path, my_rwzk_path)) {
        rs = rwvcs_rwzk_get(ti->rwvcs, rwzk_path, &rwzk_get_data);

        rwmsg_broker_connect_to_peer(bro, rwzk_get_data);
      }
      free(children[i]);
      i++;
    }
    free(children);
  }

  rs = rwvcs_rwzk_unlock_path(ti->rwvcs, rwzk_LOCK);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  free(rwzk_get_data);
  return;
}

rwmsg_broker_t *rwmsg_broker_create(uint32_t sysid,
                                    uint32_t bro_instid,
                                    char *ext_ip_address,
                                    rwsched_instance_ptr_t rws,
                                    rwsched_tasklet_ptr_t tinfo,
                                    rwvcs_zk_module_ptr_t rwvcs_zk,
                                    uint32_t use_mainq,
                                    rwmsg_endpoint_t *ep,
                                    rwtasklet_info_t *rwtasklet_info)
{
  rwmsg_broker_t *bro;

  RW_ASSERT(rws);
  RW_CF_TYPE_VALIDATE(rws, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(tinfo, rwsched_tasklet_ptr_t);
  RW_ASSERT_TYPE(ep, rwmsg_endpoint_t);

  rwmsg_channel_load_funcs(brofuncs, sizeof(brofuncs)/sizeof(brofuncs[0]));

  bro = (rwmsg_broker_t*)RW_MALLOC0_TYPE(sizeof(*bro), rwmsg_broker_t);
  bro->refct = 1;

  bro->ep = ep; 
  ck_pr_inc_32(&ep->refct);
  bro->rwtasklet_info = rwtasklet_info;

  /* By definition... */
  rwmsg_endpoint_set_property_int(bro->ep, "/rwmsg/broker/enable", 1);

  bro->tinfo = tinfo;

  RWMSG_TRACE(bro->ep, INFO, "rwmsg_broker_create(bro_instid=%u, use_mainq=%u)", bro_instid, use_mainq);

  bro->use_mainq = !!use_mainq;
  bro->chanid_nxt = 1;

  /* Crank socket limits in broker.  We rely on the system fd max to
     itself be high, as we can only crank it up to that max value. */
  {
    struct rlimit orlim = { 0 }, nrlim;
    getrlimit(RLIMIT_NOFILE, &orlim);
    memcpy(&nrlim, &orlim, sizeof(nrlim));
    nrlim.rlim_cur = nrlim.rlim_max;
    setrlimit(RLIMIT_NOFILE, &nrlim);
    getrlimit(RLIMIT_NOFILE, &nrlim);
    /* Be loud about this for a while so I can track down where and why we get into trouble in sockset/nn layers. */
    RWMSG_TRACE(bro->ep, WARN, "rwmsg_broker_create bumped RLIMIT_NOFILE from %u to %u (max=%u)",
		(unsigned int)orlim.rlim_cur, (unsigned int)nrlim.rlim_cur, (unsigned int)nrlim.rlim_max);
  }

  /* Last, as it will register for socket events */

  int port = 0;
  char brouri[999];
  rw_status_t rs;

  int thr;
  rs = rwmsg_endpoint_get_property_int(bro->ep, "/rwmsg/broker/thresh_reqct1", &thr);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  bro->thresh_reqct1 = (uint32_t)thr;
  rs = rwmsg_endpoint_get_property_int(bro->ep, "/rwmsg/broker/thresh_reqct2", &thr);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  bro->thresh_reqct2 = (uint32_t)thr;

  rs = rwmsg_endpoint_get_property_string(bro->ep, "/rwmsg/broker/nnuri", brouri, sizeof(brouri));
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  char *pnum = strrchr(brouri, ':');
  RW_ASSERT(pnum);
  pnum++;
  RW_ASSERT(pnum);
  port = atoi(pnum);
  RWMSG_TRACE(bro->ep, DEBUG, "rwmsg_broker acceptor init '/rwmsg/broker/nnuri'=%s, port=%d", brouri, port);
#ifdef RWMSG_ENABLE_UID_SEPERATION
  unsigned int  acceptor_addr=0;
  char brouri_host_str[999]; brouri_host_str[0]='\0';
  {
    char *host = strchr(brouri, ':');  // to extract host-ip from  "tcp://<host-ip>:<port>
    RW_ASSERT(host);
    host++; // skip "://" to get to the starting of host-ip
    host++;
    host++;
    strncpy(brouri_host_str, host, (pnum-host)-1);
    brouri_host_str[(pnum-host)-1] = '\0';
    if ((!ext_ip_address || (ext_ip_address && !strcmp(ext_ip_address, brouri_host_str))) &&
        inet_pton(AF_INET, brouri_host_str, &acceptor_addr)) {
      inet_ntop(AF_INET, (void*)&acceptor_addr, brouri_host_str, sizeof(brouri_host_str));
    }
  }
  RWMSG_TRACE(bro->ep, DEBUG, "acceptor_addr=0x%x %s\n", ntohl(acceptor_addr), brouri_host_str);
  rs = rwmsg_broker_acceptor_init(&bro->acc, bro, ntohl(acceptor_addr), port);
#else
  rs = rwmsg_broker_acceptor_init(&bro->acc, bro, 0, port);
#endif
  if (rs != RW_STATUS_SUCCESS) {
#if 1
    {
      FILE *in;
      extern FILE *popen();
      char buff[512];

      /* popen creates a pipe so we can read the output
      of the program we are invoking */
      if ((in = popen("netstat -ntapl", "r"))) {
        /* read the output of netstat, one line at a time */
        fprintf(stderr, "Output: netstat -ntapl\n");
        while (fgets(buff, sizeof(buff), in) != NULL ) {
          fprintf(stderr, "Output: %s", buff);
        }

        /* close the pipe */
        pclose(in);
      }
    }
#endif
    //RWMSG_TRACE(bro->ep, ERROR, "rwmsg_broker_create failure bro_instid=%u, use_mainq=%u nnuri=%s\n", bro_instid, use_mainq, brouri);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  else {

    /* Now Register in RWZK */
    if (rwvcs_zk) {
      char rwzk_path[999];
      char rwzk_LOCK[999];
      char my_rwzk_path[999];
      char rwzk_set_data[9999];
      struct timeval tv = { .tv_sec = 120, .tv_usec = 1000 };

      bro->rwvcs_zk = rwvcs_zk;
      bro->sysid = sysid;
      bro->bro_instid = bro_instid;
      rwtasklet_info_t *ti = bro->rwtasklet_info;

#if 1
      if (ti) {
        char ** children = NULL;;
        char *rwzk_get_data;
        sprintf(rwzk_LOCK, "/sys/rwmsg/broker-lock/lock-%u", bro->sysid);
        if (!rwvcs_rwzk_exists(ti->rwvcs, rwzk_LOCK)) {
          rs = rwvcs_rwzk_create(ti->rwvcs, rwzk_LOCK);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
        }
        RW_ASSERT(rwvcs_rwzk_exists(ti->rwvcs, rwzk_LOCK));
        rs = rwvcs_rwzk_lock_path(ti->rwvcs, rwzk_LOCK, &tv);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);

        sprintf(my_rwzk_path, "/sys-%u/rwmsg/broker/inst-%u", bro->sysid, bro->bro_instid);
        if (rwvcs_rwzk_exists(ti->rwvcs, my_rwzk_path)) {
          RWMSG_TRACE(bro->ep, ERROR, "ERROR: Entry for this broker already exist in RWZK (zookeeper/zake) - /sys-%u/rwmsg/broker/inst-%u\n"
                          "ERROR: Check whether a previous instance of zookeeper is already running?\n",
                          bro->sysid, bro->bro_instid);
        //  RW_ASSERT(!rwvcs_rwzk_exists(ti->rwvcs, my_rwzk_path));
        }
        else {
        RW_ASSERT(!rwvcs_rwzk_exists(ti->rwvcs, my_rwzk_path));

        rs = rwvcs_rwzk_create(ti->rwvcs, my_rwzk_path);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
        }

        if (ext_ip_address) {
          sprintf(rwzk_set_data, ":%u:tcp://%s:%u", 0, ext_ip_address, port);
        }
        else {
          sprintf(rwzk_set_data, ":%u:%s", 0, brouri);
        }
        RWMSG_TRACE(bro->ep, DEBUG, "rwzk_set_data=[%s]", rwzk_set_data);
        rs = rwvcs_rwzk_set(ti->rwvcs, my_rwzk_path, rwzk_set_data);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);

        sprintf(rwzk_path, "/sys-%u/rwmsg/broker", bro->sysid);
        rs = rwvcs_rwzk_get_children(ti->rwvcs, rwzk_path, &children);
        RW_ASSERT(rs == RW_STATUS_SUCCESS || rs == RW_STATUS_NOTFOUND);
        if (children) {
          int i=0;
          while (children[i] != NULL) {
            sprintf(rwzk_path, "/sys-%u/rwmsg/broker/%s", bro->sysid, children[i]);
            if (strcmp(rwzk_path, my_rwzk_path)) {
              rs = rwvcs_rwzk_get(ti->rwvcs, rwzk_path, &rwzk_get_data);
              RW_ASSERT(rs == RW_STATUS_SUCCESS);
              char *p = rwzk_get_data;
              char *pcount_b = strchr(p, ':');
              RW_ASSERT(pcount_b);
              pcount_b++;
              p = pcount_b;
              char *pcount_e = strchr(p, ':');
              RW_ASSERT(pcount_e);
              *pcount_e = '\0';
              pcount_e++;
              p = pcount_e;
              int count = atoi(pcount_b+1);

              char *pnum = strrchr(p, ':');
              RW_ASSERT(pnum);
              pnum++;
              RW_ASSERT(pnum);

              sprintf(rwzk_path, "/sys-%u/rwmsg/broker/%s", bro->sysid, children[i]);
              rs = rwvcs_rwzk_get(ti->rwvcs, rwzk_path, &rwzk_get_data);

              rwmsg_broker_connect_to_peer(bro, rwzk_get_data);

              count++;
              sprintf(rwzk_set_data, ":%u:%s", count, pcount_e);
              rs = rwvcs_rwzk_set(ti->rwvcs, rwzk_path, rwzk_set_data);
              RW_ASSERT(rs == RW_STATUS_SUCCESS);

            }
            free(children[i]);
            i++;
          }
          free(children);
        }
#endif
        bro->closure = rwvcs_rwzk_watcher_start(
            ti->rwvcs, 
            rwzk_path, 
            bro->tinfo,
            rwsched_dispatch_get_main_queue(bro->ep->rwsched),
            &rwmsg_broker_data_watcher_f, 
            (void *)bro);
        RW_ASSERT(bro->closure);
        rs = rwvcs_rwzk_unlock_path(ti->rwvcs, rwzk_LOCK);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
      }
    }

    if (!rwmsg_broker_g.abroker) {
      rwmsg_broker_g.abroker = bro;
    }
    ck_pr_inc_32(&rwmsg_broker_g.count);
  }
  return bro;
}


/* Awful placeholder broadcast pause/resume notification from srvchans. */
static void rwmsg_broker_bcast_pausecli_f(void *ctx) {
  rwmsg_broker_t *bro = (rwmsg_broker_t *)ctx;
  rwmsg_broker_channel_t *bch, *tmp;
  HASH_ITER(hh, bro->acc.bch_hash, bch, tmp) {
    if (bch->ch.chantype == RWMSG_CHAN_BROCLI) {
      rwsched_dispatch_async_f(bro->tinfo, bch->rwq, bch, rwmsg_broker_clichan_pause_f);
    }
  }
}
void rwmsg_broker_bcast_pausecli(rwmsg_broker_channel_t *bch) {
  RWMSG_TRACE(bch->bro->ep, DEBUG, "rwmsg_broker_bcast_pausecli(bch chanid=%u)", bch->ch.chanid);
  rwsched_dispatch_queue_t rwq = rwsched_dispatch_get_main_queue(bch->bro->ep->rwsched);
  rwsched_dispatch_async_f(bch->bro->tinfo, rwq, bch->bro, rwmsg_broker_bcast_pausecli_f);
}
static void rwmsg_broker_bcast_resumecli_f(void *ctx) {
  rwmsg_broker_t *bro = (rwmsg_broker_t *)ctx;
  rwmsg_broker_channel_t *bch, *tmp;
  HASH_ITER(hh, bro->acc.bch_hash, bch, tmp) {
    if (bch->ch.chantype == RWMSG_CHAN_BROCLI) {
      rwsched_dispatch_async_f(bro->tinfo, bch->rwq, bch, rwmsg_broker_clichan_resume_f);
    }
  }
}
void rwmsg_broker_bcast_resumecli(rwmsg_broker_channel_t *bch) {
  RWMSG_TRACE(bch->bro->ep, DEBUG, "rwmsg_broker_bcast_resumecli(bch chanid=%u)", bch->ch.chanid);
  rwsched_dispatch_queue_t rwq = rwsched_dispatch_get_main_queue(bch->bro->ep->rwsched);
  rwsched_dispatch_async_f(bch->bro->tinfo, rwq, bch->bro, rwmsg_broker_bcast_resumecli_f);
}
