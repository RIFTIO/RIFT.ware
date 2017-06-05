/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwmsg_endpoint.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG messaging endpoint object
 */

/* Need PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP */
#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <ctype.h>

#include <rwlib.h>
#include <rw_sys.h>

#include <rw-manifest.pb-c.h>

#include "rwmsg_int.h"
#include "rwmsg_broker.h"
#include "rwlog.h"

/* Forwards */
static void rwmsg_endpoint_nn_rwsched_event_file(rwmsg_endpoint_t *ep, int revents);
static void rwmsg_endpoint_nn_rwsched_event_timer(void *ud);
static int rwmsg_endpoint_nn_tasklet_yield(int fd, int events, int timeout);

/* Default properties */
static struct rwmsg_endpoint_prop_t rwmsg_default_properties[] = {
  { "/rwmsg/broker/enable", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/broker/shunt", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/broker/thresh_reqct1", RWMSG_EP_PROPTYPE_INT32, .ival=900 }, /* xon */
  { "/rwmsg/broker/thresh_reqct2", RWMSG_EP_PROPTYPE_INT32, .ival=1000 }, /* xoff */
  { "/rwmsg/broker/nnuri", RWMSG_EP_PROPTYPE_STRING, .sval=BASE_BROKER_URL },
  { "/rwmsg/broker/srvchan/window", RWMSG_EP_PROPTYPE_INT32, .ival=250 },

  { "/rwmsg/queue/broclichan/qlen", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/queue/broclichan/qsz", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/queue/brosrvchan/qlen", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/queue/brosrvchan/qsz", RWMSG_EP_PROPTYPE_INT32, .ival=0 },

  { "/rwmsg/queue/peerclichan/qlen", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/queue/peerclichan/qsz", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/queue/peersrvchan/qlen", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/queue/peersrvchan/qsz", RWMSG_EP_PROPTYPE_INT32, .ival=0 },

  { "/rwmsg/queue/cc_ssbuf/qlen", RWMSG_EP_PROPTYPE_INT32, .ival=0 },
  { "/rwmsg/queue/cc_ssbuf/qsz", RWMSG_EP_PROPTYPE_INT32, .ival=0 },

  { "/rwmsg/destination/defwin", RWMSG_EP_PROPTYPE_INT32, .ival=1 }, /* default window in destination object, slight chance of reordering if over 1, see note in brosrvchan */

  { "/rwmsg/signature/defto/0", RWMSG_EP_PROPTYPE_INT32, .ival=60*100 },
  { "/rwmsg/signature/defto/1", RWMSG_EP_PROPTYPE_INT32, .ival=60*100 },
  { "/rwmsg/signature/defto/2", RWMSG_EP_PROPTYPE_INT32, .ival=60*100 },
  { "/rwmsg/signature/defto/3", RWMSG_EP_PROPTYPE_INT32, .ival=60*100 },
  { "/rwmsg/signature/defto/4", RWMSG_EP_PROPTYPE_INT32, .ival=60*100 },

  { "/rwmsg/channel/ageout_time", RWMSG_EP_PROPTYPE_INT32, .ival=60 },

  //...

  { NULL }
};

/* Processwide data.  Stats, primary hash tables, etc. */
struct rwmsg_endpoint_global_s rwmsg_global = { .rgmut = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP };


/* Allocator for ck hash tables */
static void *ht_malloc(size_t r) {
  return RW_MALLOC(r);
}
static void ht_free(void*p, size_t b, bool r) {
  b=b;
  r=r;
  free(p);
}
static struct ck_malloc rwmsg_ckmalloc = {
  .malloc = ht_malloc,
  .free = ht_free
};

static void rwmsg_update_nnuri(rwmsg_endpoint_t *ep,
                               uint16_t port,
                               const char *host) {
  char tmp[999];
  char tmpport[32];
  char *pnum;

  if (port) {
    rwmsg_endpoint_get_property_string(ep,
                                       "/rwmsg/broker/nnuri",
                                       tmp,
                                       sizeof(tmp));
    pnum = strrchr(tmp, ':');
    RW_ASSERT(pnum);
    pnum++;
    sprintf(pnum, "%d", port+ep->bro_instid);
    rwmsg_endpoint_set_property_string(ep, "/rwmsg/broker/nnuri", tmp);
    //    printf("nnuri=%s\n", tmp);
  }

  if (host) {
    rwmsg_endpoint_get_property_string(ep,
                                       "/rwmsg/broker/nnuri",
                                       tmp,
                                       sizeof(tmp));
    pnum = strrchr(tmp, ':');
    RW_ASSERT(pnum);
    pnum++;
    strcpy(tmpport, pnum);
    sprintf(tmp, "tcp://%s:%s", host, tmpport);
    rwmsg_endpoint_set_property_string(ep, "/rwmsg/broker/nnuri", tmp);
    //    printf("nnuri=%s\n", tmp);
  }
}

int rwmsg_nn_direct_cb(int fd);

rwmsg_endpoint_t *rwmsg_endpoint_create(uint32_t sysid,
                    uint32_t instid,
                    uint32_t bro_instid,
                    rwsched_instance_ptr_t rws,
                    rwsched_tasklet_ptr_t tinfo,
                    rwtrace_ctx_t *rwtctx,
                    struct RwManifest__YangData__RwManifest__Manifest__InitPhase__Settings__Rwmsg *mani) {
  RW_ASSERT(rws);
  RW_CF_TYPE_VALIDATE(rws, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(tinfo, rwsched_tasklet_ptr_t);

  rwmsg_endpoint_t *ep = NULL;
  char tmp[999];

  /* TBD Use tasklet-specific data to enforce one endpoint per tasklet */

  ep = (rwmsg_endpoint_t*)RW_MALLOC0_TYPE(sizeof(*ep), rwmsg_endpoint_t);
  ep->rwtctx = rwtctx;
  ep->rwlog_instance = rwlog_init("RW.Msg");

  if (getenv("RWMSG_DEBUG")) {
    rwtrace_ctx_category_severity_set(rwtctx,
                                  RWTRACE_CATEGORY_RWMSG,
                                  RWTRACE_SEVERITY_DEBUG);
    rwtrace_ctx_category_destination_set(rwtctx,
                                     RWTRACE_CATEGORY_RWMSG,
                                     RWTRACE_DESTINATION_CONSOLE);
  } else {
#if 0
    rwtrace_ctx_category_severity_set(rwtctx,
                                  RWTRACE_CATEGORY_RWMSG,
                                  RWTRACE_SEVERITY_DEBUG);
    rwtrace_ctx_category_destination_set(rwtctx,
                                     RWTRACE_CATEGORY_RWMSG,
                                     RWTRACE_DESTINATION_CONSOLE);
#endif
  }

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&ep->epmut, &attr);

  int r = pthread_mutex_lock(&ep->epmut);
  RW_ASSERT(r==0);
  if (r) {
    RW_FREE_TYPE(ep, rwmsg_endpoint_t);
    ep = NULL;
    goto lockret;
  }
  ep->epid = ++rwmsg_global.epid_nxt;
  sprintf(ep->rwtpfx, "[epid %u on i%u]", ep->epid, instid);

  RWMSG_TRACE(ep, INFO, "Creating endpoint %p (sysid=%u instid=%u bro_instid=%u rwsched=%p taskletinfo=%p)", ep, sysid, instid, bro_instid, rws, tinfo);

  ep->rwsched = rws;
  ep->taskletinfo = tinfo;
  ep->sysid = sysid;
  ep->instid = instid;
  ep->bro_instid = bro_instid;
  ep->paths_nxt = 1;
  ep->methbind_nxt = 1;
  //...

  rwmsg_garbage_truck_init(&ep->gc, ep);

  {
    /* Add in default property values */
    struct rwmsg_endpoint_prop_t *prop = &rwmsg_default_properties[0];
    while (prop && prop->path) {
      switch (prop->valtype) {
      case RWMSG_EP_PROPTYPE_INT32:
        rwmsg_endpoint_set_property_int(ep, prop->path, prop->ival);
        break;
      case RWMSG_EP_PROPTYPE_STRING:
        rwmsg_endpoint_set_property_string(ep, prop->path, prop->sval);
        break;
      default:
        RW_CRASH();
        break;
      }
      prop++;
    }

#if 1
    {
    rw_status_t rs = rwmsg_endpoint_get_property_string(ep, "/rwmsg/broker/nnuri", tmp, sizeof(tmp));
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    char *pnum = strrchr(tmp, ':');
    RW_ASSERT(pnum);
    pnum++;
    RW_ASSERT(pnum);
    uint16_t port = atoi(pnum);
    *pnum = '\0';
    sprintf(pnum,"%u", port+bro_instid);
    //strcat(tmp, itoa(port+bro_instid));
    rwmsg_endpoint_set_property_string(ep, "/rwmsg/broker/nnuri", tmp);
    }
#endif

    uint16_t port = 0;
    /* Install settings from the rw manifest */
    if (mani) {
      if (mani->single_broker) {
        if (mani->single_broker->has_enable_broker) {
          rwmsg_endpoint_set_property_int(ep, "/rwmsg/broker/enable", mani->single_broker->enable_broker);
        }
        if (mani->single_broker->has_broker_port) {
          rwmsg_update_nnuri(ep, mani->single_broker->broker_port, NULL);
          port = mani->single_broker->broker_port;
        }
        if (mani->single_broker->broker_host) {
          rwmsg_update_nnuri(ep, 0, mani->single_broker->broker_host);
        }
      } else if (mani->multi_broker) {
        if (mani->multi_broker->has_enable) {
          rwmsg_endpoint_set_property_int(ep, "/rwmsg/broker/enable", mani->multi_broker->enable);
        }
      }
    }

    /* Environment variables for simple override in unit tests and
       until we get some non-annoying manifest override mechanism. */
    const char *usebro = getenv("RWMSG_BROKER_ENABLE");
    if (usebro) {
      rwmsg_endpoint_set_property_int(ep, "/rwmsg/broker/enable", atoi(usebro));
    }
    const char *shunt = getenv("RWMSG_BROKER_SHUNT");
    if (shunt) {
      rwmsg_endpoint_set_property_int(ep, "/rwmsg/broker/shunt", atoi(shunt));
    }
    const char *ageout_time = getenv("RWMSG_CHANNEL_AGEOUT");
    if (ageout_time) {
      rwmsg_endpoint_set_property_int(ep, "/rwmsg/channel/ageout_time", atoi(ageout_time));
    }
    const char *envport = getenv("RWMSG_BROKER_PORT");
    const char *envhost = getenv("RWMSG_BROKER_HOST");

    if (envport) {
      long int long_port;

      long_port = strtol(envport, NULL, 10);
      if (long_port < 65535 && long_port > 0)
        port = (uint16_t)long_port;
    }

    if (!port) {
      rw_status_t status;

      status = rw_unique_port(BASE_BROKER_PORT, &port);
      if (status != RW_STATUS_SUCCESS)
        port = 0;

    }

    rwmsg_update_nnuri(ep, port, envhost);

#ifdef RWMSG_ENABLE_UID_SEPERATION
  rwmsg_endpoint_get_property_string(ep, "/rwmsg/broker/nnuri", tmp, sizeof(tmp));
  if (!mani && getenv("RIFT_INSTANCE_UID") && !strncmp(BASE_BROKER_HOST, tmp, sizeof(BASE_BROKER_HOST)-1)) {
    char *pnum = strrchr(tmp, ':');
    char tmpport[32];
    RW_ASSERT(pnum);
    pnum++;
    strcpy(tmpport, pnum);
    sprintf(tmp, "tcp://%s:%s", RWMSG_CONNECT_ADDR_STR, tmpport);
    rwmsg_endpoint_set_property_string(ep, "/rwmsg/broker/nnuri", tmp);
  }
#endif

  }
  rwmsg_endpoint_get_property_string(ep, "/rwmsg/broker/nnuri", tmp, sizeof(tmp));

  RW_DL_INIT(&ep->track.queues);
  //...

  const int singlethread = FALSE;//TRUE;
  int do_nn=FALSE;

  if (1) {
    char tmp[256];
    int ena=0;
    int shunt=0;
    rwmsg_endpoint_get_property_string(ep, "/rwmsg/broker/nnuri", tmp, sizeof(tmp));
    rwmsg_endpoint_get_property_int(ep, "/rwmsg/broker/enable", &ena);
    rwmsg_endpoint_get_property_int(ep, "/rwmsg/broker/shunt", &shunt);
    RWMSG_TRACE(ep, INFO, "Broker config enable=%d shunt=%d nnuri='%s'", ena, shunt, tmp);
  }

  RWMSG_EP_UNLOCK(ep);

  ck_pr_inc_32(&ep->refct);

  /* Init or just update rwmsg_global */
  ck_pr_inc_32(&rwmsg_global.status.endpoint_ct);
  RWMSG_RG_LOCK();
  if (rwmsg_global.status.endpoint_ct == 1) {
    rwmsg_global.chanid_nxt = 1;
    bool rb = ck_ht_init(&rwmsg_global.localpathtab, CK_HT_MODE_BYTESTRING, NULL, &rwmsg_ckmalloc, 2*RWMSG_PATHTAB_MAX, 0);
    RW_ASSERT(rb);
    if (!rb) {
      abort();//RW_CRASH();
    }
    rb = ck_ht_init(&rwmsg_global.localdesttab, CK_HT_MODE_BYTESTRING, NULL, &rwmsg_ckmalloc, 5*RWMSG_METHBIND_MAX, 0);
    RW_ASSERT(rb);
    if (!rb) {
      abort();//RW_CRASH();
    }
    memset(rwmsg_global.cbs, 0, sizeof(rwmsg_global.cbs));
  }

  if (!rwmsg_global.nn_inited) {
    struct nn_riftconfig nncfg  = {
      .singlethread = singlethread,
      .waitfunc = rwmsg_endpoint_nn_tasklet_yield,
      .direct_cb = rwmsg_nn_direct_cb
    };
    RWMSG_TRACE(ep, DEBUG, "NN config .singlethread=%u .waitfunc=%p in ep=%p", nncfg.singlethread, nncfg.waitfunc, ep);
    nn_global_init(&nncfg);
    rwmsg_global.nn_inited = TRUE;
    do_nn = TRUE;
  }

  RWMSG_RG_UNLOCK();

  if (do_nn) {
    if (singlethread) {
      /* Calling with no revents merely runs the get pollset / timeout logic */
      rwmsg_endpoint_nn_rwsched_event_file(ep, 0);
    }
  }

 lockret:
  return ep;

  sysid=sysid;
}

static void rwmsg_endpoint_nn_rwsched_event_w(void *ud) {
  rwmsg_endpoint_t *ep = (rwmsg_endpoint_t*) ud;
  rwmsg_endpoint_nn_rwsched_event_file(ep, POLLOUT);
}

static void rwmsg_endpoint_nn_rwsched_event_r(void *ud) {
  rwmsg_endpoint_t *ep = (rwmsg_endpoint_t*) ud;
  rwmsg_endpoint_nn_rwsched_event_file(ep, POLLIN);
}

static void rwmsg_endpoint_nn_rwsched_event_file(rwmsg_endpoint_t *ep, int revents) {

  if (revents) {
    nn_run_worker();
  }

  nn_worker_getfdto(&ep->nn.fd, &ep->nn.events, &ep->nn.timeout);

  if (ep->nn.rwsource_fd_read) {
    rwsched_dispatch_source_cancel(ep->taskletinfo, ep->nn.rwsource_fd_read);
    //NEW-CODE
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_SRCREL, ep->nn.rwsource_fd_read, ep->rwsched, ep->taskletinfo);
    ep->nn.rwsource_fd_read = NULL;
  }
  if (ep->nn.rwsource_fd_write) {
    rwsched_dispatch_source_cancel(ep->taskletinfo, ep->nn.rwsource_fd_write);
    //NEW-CODE
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_SRCREL, ep->nn.rwsource_fd_write, ep->rwsched, ep->taskletinfo);
    ep->nn.rwsource_fd_write = NULL;
  }
  if (ep->nn.fd > -1 && (POLLIN&ep->nn.events)) {
    ep->nn.rwsource_fd_read = rwsched_dispatch_source_create(ep->taskletinfo,
                                                             RWSCHED_DISPATCH_SOURCE_TYPE_READ,
                                                             ep->nn.fd,
                                                             0,
                                                             rwsched_dispatch_get_main_queue(ep->rwsched));
    RW_ASSERT(ep->nn.rwsource_fd_read);
    rwsched_dispatch_set_context(ep->taskletinfo,ep->nn.rwsource_fd_read, ep);
    rwsched_dispatch_source_set_event_handler_f(ep->taskletinfo, ep->nn.rwsource_fd_read, rwmsg_endpoint_nn_rwsched_event_r);
  }
  if (ep->nn.fd > -1 && (POLLOUT&ep->nn.events)) {
    ep->nn.rwsource_fd_write = rwsched_dispatch_source_create(ep->taskletinfo,
                                                              RWSCHED_DISPATCH_SOURCE_TYPE_WRITE,
                                                              ep->nn.fd,
                                                              0,
                                                              rwsched_dispatch_get_main_queue(ep->rwsched));
    RW_ASSERT(ep->nn.rwsource_fd_write);
    rwsched_dispatch_set_context(ep->taskletinfo,ep->nn.rwsource_fd_write, ep);
    rwsched_dispatch_source_set_event_handler_f(ep->taskletinfo, ep->nn.rwsource_fd_write, rwmsg_endpoint_nn_rwsched_event_w);
  }

  if (ep->nn.rwsource_timer) {
    rwsched_dispatch_source_cancel(ep->taskletinfo, ep->nn.rwsource_timer);
    //NEW-CODE
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_SRCREL, ep->nn.rwsource_timer, ep->rwsched, ep->taskletinfo);
    ep->nn.rwsource_timer = NULL;
  }
  if (ep->nn.timeout > -1) {
    ep->nn.rwsource_timer = rwsched_dispatch_source_create(ep->taskletinfo,
                                                           RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                           0,
                                                           0,
                                                           rwsched_dispatch_get_main_queue(ep->rwsched));
    RW_ASSERT(ep->nn.rwsource_timer);
    uint64_t ival = ep->nn.timeout * 1000 * 1000;
    rwsched_dispatch_source_set_timer(ep->taskletinfo, ep->nn.rwsource_timer, DISPATCH_TIME_NOW, ival, ival/2);
    rwsched_dispatch_source_set_event_handler_f(ep->taskletinfo, ep->nn.rwsource_timer, rwmsg_endpoint_nn_rwsched_event_timer);
    rwsched_dispatch_set_context(ep->taskletinfo, ep->nn.rwsource_timer, ep);
    rwsched_dispatch_resume(ep->taskletinfo, ep->nn.rwsource_timer);
  }
}

static void rwmsg_endpoint_nn_rwsched_event_timer(void *ud) {
  rwmsg_endpoint_t *ep = (rwmsg_endpoint_t*) ud;

  nn_run_worker();

  nn_worker_getfdto(NULL, NULL, &ep->nn.timeout);

  if (ep->nn.rwsource_timer) {
    rwsched_dispatch_source_cancel(ep->taskletinfo, ep->nn.rwsource_timer);
    //NEW-CODE
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_SRCREL, ep->nn.rwsource_timer, ep->rwsched, ep->taskletinfo);
    ep->nn.rwsource_timer = NULL;
  }
  if (ep->nn.timeout > -1) {
    ep->nn.rwsource_timer = rwsched_dispatch_source_create(ep->taskletinfo,
                                                           RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                           0,
                                                           0,
                                                           rwsched_dispatch_get_main_queue(ep->rwsched));
    RW_ASSERT(ep->nn.rwsource_timer);
    uint64_t ival = ep->nn.timeout * 1000 * 1000;
    rwsched_dispatch_source_set_timer(ep->taskletinfo, ep->nn.rwsource_timer, DISPATCH_TIME_NOW, ival, ival/2);
    rwsched_dispatch_source_set_event_handler_f(ep->taskletinfo, ep->nn.rwsource_timer, rwmsg_endpoint_nn_rwsched_event_timer);
    rwsched_dispatch_set_context(ep->taskletinfo, ep->nn.rwsource_timer, ep);
    rwsched_dispatch_resume(ep->taskletinfo, ep->nn.rwsource_timer);
  }
}

static int rwmsg_endpoint_nn_tasklet_yield(int fd, int events, int timeout) {

  /* This only happens for blocking nn socket operations.  Ixnay on that! */
  RW_CRASH();

  // block on e->nn.rwsource_fd_read?
  // which ep?
  //rwmsg_endpoint_nn_rwsched_event_file(ep, 0);
  return POLLIN;
  fd=fd;
  events=events;
  timeout=timeout;
}


rwmsg_bool_t rwmsg_endpoint_halt_flush(rwmsg_endpoint_t *ep, int flush) {
  rwmsg_bool_t retval = FALSE;

  if (ep->lastreq) {
    rwmsg_request_release(ep->lastreq);
    ep->lastreq = NULL;
  }

  rwmsg_garbage_truck_flush(&ep->gc, flush);

  rwmsg_endpoint_trace_status(ep);

  /*
  RW_ASSERT(ep->stat.objects.notify_eventfds == 0);
  uint32_t nn_eventfds = ck_pr_load_32(&ep->stat.objects.nn_eventfds);
  RW_ASSERT(nn_eventfds == 0);
  */

  retval = rwmsg_endpoint_release(ep);
  return retval;
}

rwmsg_bool_t rwmsg_endpoint_halt(rwmsg_endpoint_t *ep) {
  return rwmsg_endpoint_halt_flush(ep, FALSE);
}

void rwmsg_garbage_truck_flush(rwmsg_garbage_truck_t *gc, int maindrain) {
  RWMSG_EP_LOCK(gc->ep);
  /* Delete everything in the garbage queue.  There should be no
     references to these items as everything is gone. */
  int gbmax=10000;
  do {
    RWMSG_EP_UNLOCK(gc->ep);
    RWMSG_SLEEP_MS(50);
    rwmsg_garbage_truck_tick(gc);
    if (maindrain) {
      dispatch_main_queue_drain_np();
    }
    RWMSG_EP_LOCK(gc->ep);
  } while (RW_DL_LENGTH(&gc->garbageq) && gbmax-- > 0);

  RWMSG_EP_UNLOCK(gc->ep);
  return;
}

rwmsg_bool_t rwmsg_endpoint_release(rwmsg_endpoint_t *ep) {

  bool zero=0;
  ck_pr_dec_32_zero(&ep->refct, &zero);
  if (zero) {
    RWMSG_SLEEP_MS(50);        /* fugly, switch gc to rwmsg_global */
    ck_pr_barrier();
    if (ep->refct) {
      goto retnah;
    }
    RW_ASSERT(!ep->refct);

    rwmsg_garbage_truck_flush(&ep->gc, FALSE);

    RWMSG_EP_LOCK(ep);

    RW_ASSERT(RW_DL_LENGTH(&ep->gc.garbageq) == 0);

    if (ep->gc.rws_timer) {
      rwsched_dispatch_source_cancel(ep->taskletinfo, ep->gc.rws_timer);
      //NEW-CODE
      rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_SRCREL, ep->gc.rws_timer, ep->rwsched, ep->taskletinfo);
      ep->gc.rws_timer = NULL;
    }

    // fsck, need dispatch_sync_f() and guaranteed progress on mainq, which we can't have
    RWMSG_SLEEP_MS(100);

    ep->rwsched = NULL;
    ep->taskletinfo = NULL;

    RW_ASSERT(rwmsg_global.status.endpoint_ct);
    zero=0;
    ck_pr_dec_32_zero(&rwmsg_global.status.endpoint_ct, &zero);
    if (zero) {
      ck_ht_destroy(&rwmsg_global.localdesttab);
      ck_ht_destroy(&rwmsg_global.localpathtab);
      memset(rwmsg_global.cbs, 0, sizeof(rwmsg_global.cbs));
    }

    if (ep->localprops) {
      int i;
      for (i=0; i<ep->localprops_ct; i++) {
    struct rwmsg_endpoint_prop_t *prop=NULL;
    prop = &ep->localprops[i];
    RW_ASSERT(prop->path);
    free(prop->path);
    prop->path = NULL;
    if (prop->valtype == RWMSG_EP_PROPTYPE_STRING) {
      free(prop->sval);
      prop->sval = NULL;
    }
      }
      ep->localprops_ct=0;
      RW_FREE(ep->localprops);
      ep->localprops=NULL;
    }
    if (ep->rwlog_instance) {
      rwlog_close(ep->rwlog_instance, FALSE);
      ep->rwlog_instance = NULL;
    }
    RWMSG_EP_UNLOCK(ep);
    RW_FREE_TYPE(ep, rwmsg_endpoint_t);
    return TRUE;
  } else {
  retnah:
    return FALSE;
  }
}

void rwmsg_endpoint_set_property_int(rwmsg_endpoint_t *ep,
                                     const char *ppath,
                                     int32_t i_in) {
  /* Broker TBD, send over utility channel! */
  RWMSG_EP_LOCK(ep);
  struct rwmsg_endpoint_prop_t *prop=NULL;
  int i;
  for (i=0; i<ep->localprops_ct; i++) {
    if (0==strcmp(ep->localprops[i].path, ppath)) {
      prop = &ep->localprops[i];
      break;
    }
  }
  if (!prop) {
    ep->localprops = (struct rwmsg_endpoint_prop_t *)realloc(ep->localprops, (1+ep->localprops_ct) * sizeof(struct rwmsg_endpoint_prop_t));
    prop = &ep->localprops[ep->localprops_ct++];
    memset(prop, 0, sizeof(*prop));
  }
  if (!prop->path) {
    prop->path = strdup(ppath);
  } else {
    if (prop->valtype == RWMSG_EP_PROPTYPE_STRING) {
      free(prop->sval);
      prop->sval = NULL;
    }
  }
  prop->valtype = RWMSG_EP_PROPTYPE_INT32;
  prop->ival = i_in;
  RWMSG_EP_UNLOCK(ep);
  return;
}

rw_status_t rwmsg_endpoint_get_property_int(rwmsg_endpoint_t *ep,
                        const char *ppath,
                        int32_t *i_out) {
  /* Broker TBD, shm tab and/or utility channel! */
  RWMSG_EP_LOCK(ep);
  rw_status_t retval = RW_STATUS_FAILURE;

  RW_ASSERT(i_out);
  RW_ASSERT(ppath);

  struct rwmsg_endpoint_prop_t *prop=NULL;
  int i;
  for (i=0; i<ep->localprops_ct; i++) {
    if (0==strcmp(ep->localprops[i].path, ppath)) {
      prop = &ep->localprops[i];
      break;
    }
  }
  if (!prop) {
    retval = RW_STATUS_NOTFOUND;
    goto ret;
  }

  if (prop->valtype != RWMSG_EP_PROPTYPE_INT32) {
    retval = RW_STATUS_OUT_OF_BOUNDS;
    goto ret;
  }

  *i_out = prop->ival;
  retval = RW_STATUS_SUCCESS;

 ret:
  RWMSG_EP_UNLOCK(ep);
  return retval;
}


void rwmsg_endpoint_set_property_string(rwmsg_endpoint_t *ep,
                    const char *ppath,
                    const char *str_in) {
  /* Broker TBD, send over utility channel! */
  RWMSG_EP_LOCK(ep);

  struct rwmsg_endpoint_prop_t *prop=NULL;
  int i;
  for (i=0; i<ep->localprops_ct; i++) {
    if (0==strcmp(ep->localprops[i].path, ppath)) {
      prop = &ep->localprops[i];
      break;
    }
  }
  if (!prop) {
    ep->localprops = (struct rwmsg_endpoint_prop_t *)realloc(ep->localprops, (1+ep->localprops_ct) * sizeof(struct rwmsg_endpoint_prop_t));
    prop = &ep->localprops[ep->localprops_ct++];
    memset(prop, 0, sizeof(*prop));
  }
  if (!prop->path) {
    prop->path = strdup(ppath);
  } else {
    if (prop->valtype == RWMSG_EP_PROPTYPE_STRING) {
      free(prop->sval);
      prop->sval = NULL;
    }
  }
  prop->valtype = RWMSG_EP_PROPTYPE_STRING;
  prop->sval = strdup(str_in);

  RWMSG_EP_UNLOCK(ep);
  return;
}

rw_status_t rwmsg_endpoint_get_property_string(rwmsg_endpoint_t *ep,
                           const char *ppath,
                           char *str_out, int str_out_len) {
  /* Broker TBD, shm tab and/or utility channel! */
  RWMSG_EP_LOCK(ep);
  rw_status_t retval = RW_STATUS_FAILURE;

  RW_ASSERT(str_out);
  RW_ASSERT(str_out_len > 0);
  RW_ASSERT(ppath);

  struct rwmsg_endpoint_prop_t *prop=NULL;
  int i;
  for (i=0; i<ep->localprops_ct; i++) {
    if (0==strcmp(ep->localprops[i].path, ppath)) {
      prop = &ep->localprops[i];
      break;
    }
  }
  if (!prop) {
    retval = RW_STATUS_NOTFOUND;
    goto ret;
  }

  if (prop->valtype != RWMSG_EP_PROPTYPE_STRING) {
    retval = RW_STATUS_OUT_OF_BOUNDS;
    goto ret;
  }

  strncpy(str_out, prop->sval, str_out_len-1);
  str_out[str_out_len-1]='\0';
  retval = RW_STATUS_SUCCESS;

 ret:
  RWMSG_EP_UNLOCK(ep);
  return retval;
}

rwsched_instance_ptr_t rwmsg_endpoint_get_rwsched(rwmsg_endpoint_t *ep) {
  return ep->rwsched;
}

void rwmsg_endpoint_get_status_FREEME(rwmsg_endpoint_t *ep,
                      rwmsg_endpoint_status_t **status_out) {
  *status_out = NULL;
  RWMSG_EP_LOCK(ep);
  *status_out = RW_MALLOC0(sizeof(rwmsg_endpoint_status_t));
  memcpy(*status_out, &ep->stat, sizeof(rwmsg_endpoint_status_t));
  /* TBD fill in detail with all the individual objects' info */
  RWMSG_EP_UNLOCK(ep);
  return;
}

void rwmsg_endpoint_get_status_free(rwmsg_endpoint_status_t *trash) {
  if (trash->detail) {
    RW_FREE(trash->detail);
    trash->detail = NULL;
  }
  RW_FREE(trash);
}

void rwmsg_endpoint_trace_status(rwmsg_endpoint_t *ep) {
  rwmsg_endpoint_status_t *s = &ep->stat;

#define PP(x) do { if (s->objects.x) { RWMSG_TRACE(ep, INFO, "%s count: %u", #x, (unsigned)s->objects.x); } } while (0)
  PP(destinations);
  PP(methods);
  PP(signatures);
  PP(requests);
  PP(queues);
  PP(srvchans);
  PP(clichans);
  PP(brosrvchans);
  PP(broclichans);
  PP(peersrvchans);
  PP(peerclichans);
  PP(socksets);
}

/* Call rwmsg_endpoint_release() when done with the path! */
rwmsg_endpoint_t *rwmsg_endpoint_path_localfind_ref(rwmsg_endpoint_t *ep,
                            const char *path) {
  rwmsg_endpoint_t *epret = NULL;

  size_t plen = strlen(path);
  if (plen+1+16/*SID#####*/ > RWMSG_PATH_MAX) {
    goto ret;
  }
  if (plen < 4) {
    RW_ASSERT(plen >= 4);
    goto ret;
  }
  if (path[0] != '/') {
    RW_ASSERT(path[0] == '/');
    goto ret;
  }

  /* Lockless lookup of path in process-global table. */
  {
    int r;
    ck_ht_entry_t hent;
    ck_ht_hash_t hash;

    ck_ht_hash(&hash, &rwmsg_global.localpathtab, path, plen);
    ck_ht_entry_key_set(&hent, path, plen);
    r = ck_ht_get_spmc(&rwmsg_global.localpathtab, hash, &hent);
    if (r) {
      epret = (rwmsg_endpoint_t*)ck_ht_entry_value(&hent);
      const char *k = (const char *)ck_ht_entry_key(&hent);
      RW_ASSERT(0==strcmp(k, path));
    }
  }

  if (epret) {
    ck_pr_inc_32(&epret->refct);
  }

 ret:
  return epret;

  ep=ep;
}

/*
 * Srvchan calls this to find the method proper.  Ought to be a
 * srvchan-local index of methods, but this is good enough for now.
 *
 * @return Returns NULL, or a rwmsg_method_t on which a reference is
 * taken.  Call rwmsg_method_release when done with the method.
 */
rwmsg_method_t *rwmsg_endpoint_find_method(rwmsg_endpoint_t *ep,
                       uint64_t pathhash,
                       rwmsg_payfmt_t payt,
                       uint32_t methno) {
  struct rwmsg_methbinding_key_s key;
  memset(&key, 0, sizeof(key));
  key.bindtype = RWMSG_METHB_TYPE_SRVCHAN;
  key.pathhash = pathhash;
  key.payt = payt;
  key.methno = methno;

  int r;
  ck_ht_entry_t hent;
  ck_ht_hash_t hash;

  ck_ht_hash(&hash, &rwmsg_global.localdesttab, &key, sizeof(key));
  ck_ht_entry_key_set(&hent, &key, sizeof(key));
  r = ck_ht_get_spmc(&rwmsg_global.localdesttab, hash, &hent);
  if (r) {
    struct rwmsg_methbinding_s *mb = (struct rwmsg_methbinding_s*)ck_ht_entry_value(&hent);
    if (mb->srvchans_ct) {
#if 1
      if (ep == mb->ep) {
        rwmsg_method_t *meth = mb->meth;
        ck_pr_inc_32(&meth->refct);
        return meth;
      } else {
#endif
        RWMSG_RG_LOCK();
        {
          ck_ht_iterator_t iter = CK_HT_ITERATOR_INITIALIZER;
          ck_ht_entry_t *ent=NULL;
          while (ck_ht_next(&rwmsg_global.localdesttab, &iter, &ent)) {
            RW_ASSERT(ent);
            struct rwmsg_methbinding_s *mb = (struct rwmsg_methbinding_s *)ent->value;
            RW_ASSERT(mb->srvchans_ct<=(sizeof(mb->srvchans)/sizeof(mb->srvchans[0])));
            if (mb->ep == ep) {
              rwmsg_method_t *meth = mb->meth;
              if (meth
                  && meth->pathhash == pathhash
                  && meth->sig
                  && meth->sig->payt == payt
                  && meth->sig->methno == methno) {
                RWMSG_RG_UNLOCK();
                ck_pr_inc_32(&meth->refct);
                return meth;
              }
            }
            ent = NULL;
          }
        }
        RWMSG_RG_UNLOCK();
      }
    }
  }

  return NULL;
}

/*
 * Clichan or broclichan call this to get a local binding to a
 * srvchan anywhere in this process.  It returns NULL, or a
 * rwmsg_srvchan_t on which a reference is taken.  Call
 * rwmsg_srvchan_release when done with the sc.
 */
rwmsg_channel_t *rwmsg_endpoint_find_channel(rwmsg_endpoint_t *ep,
                                             enum rwmsg_methb_type_e btype,
                                             uint32_t bro_intsid,
                                             uint64_t pathhash,
                                             rwmsg_payfmt_t payt,
                                             uint32_t methno) {
  struct rwmsg_methbinding_key_s key;
  memset(&key, 0, sizeof(key));
  key.bindtype = btype;
  if (btype != RWMSG_METHB_TYPE_SRVCHAN)
    key.bro_instid = bro_intsid;
  key.pathhash = pathhash;
  key.payt = payt;
  key.methno = methno;

  rwmsg_channel_t *ch = NULL;

  int r;
  ck_ht_entry_t hent;
  ck_ht_hash_t hash;

  ck_ht_hash(&hash, &rwmsg_global.localdesttab, &key, sizeof(key));

  ck_ht_entry_key_set(&hent, &key, sizeof(key));
  r = ck_ht_get_spmc(&rwmsg_global.localdesttab, hash, &hent);
  if (r) {
    struct rwmsg_methbinding_s *mb = (struct rwmsg_methbinding_s*)ck_ht_entry_value(&hent);
    ck_pr_inc_32(&mb->stat.found_called);
    if (mb->srvchans_ct) {
      /* TBD RR/HASH across multiple srvchans, per coherency value */
      ch = mb->srvchans[0].ch;
      _RWMSG_CH_DEBUG_(ch, "++");
      ck_pr_inc_32(&ch->refct);
      if (ep != mb->ep) {
    /* Different tasklet.  Don't think we care. */
      }
      RW_ASSERT(mb->ep == ch->ep);
    }
  }

  if (1) {
    RWMSG_TRACE(ep, DEBUG, "lookup phash=0x%lx payt %u methno %u hash %lx (bro-instid %u btype %u) -> ch %p %s", key.pathhash, key.payt, key.methno, hash.value, bro_intsid, btype, ch, (ch? ch->rwtpfx : ""));
  }

  return ch;
}


/*
 * Delete all method bindings to a channel.
 */
rwmsg_bool_t rwmsg_endpoint_del_channel_method_bindings(rwmsg_endpoint_t *ep,
                            rwmsg_channel_t *ch) {
  const int mmax=500;
  int mct=0;
  rwmsg_method_t *relmeth[mmax];
  memset(relmeth, 0, sizeof(relmeth));

  RWMSG_RG_LOCK();
  {
    ck_ht_iterator_t iter = CK_HT_ITERATOR_INITIALIZER;
    ck_ht_entry_t *ent=NULL;
    while (ck_ht_next(&rwmsg_global.localdesttab, &iter, &ent)) {
      RW_ASSERT(ent);
      struct rwmsg_methbinding_s *mb = (struct rwmsg_methbinding_s *)ent->value;
      uint32_t i;
      RW_ASSERT(mb->srvchans_ct<=(sizeof(mb->srvchans)/sizeof(mb->srvchans[0])));
      for (i=0; i<mb->srvchans_ct; i++) {
        if (mb->srvchans[i].ch == ch) {
          if (mb->srvchans_ct > 1 &&
              i < mb->srvchans_ct-1 &&
              mb->srvchans_ct <= (sizeof(mb->srvchans)/sizeof(mb->srvchans[0]))) {
            mb->srvchans[i] = mb->srvchans[mb->srvchans_ct-1];
            /* Temporary duplicate entry in srvchans until we hit ct-- */
          }
          mb->srvchans_ct--;
        }
        if (!mb->srvchans_ct) {
          ck_ht_entry_t entcpy;
          memcpy(&entcpy, ent, sizeof(entcpy));
          entcpy.value = 0;
          ck_ht_hash_t h;
          h.value = entcpy.hash;
          ck_ht_remove_spmc(&rwmsg_global.localdesttab, h, &entcpy);
          RW_ASSERT((struct rwmsg_methbinding_s *)entcpy.value == mb);

          relmeth[mct++] = mb->meth;

          if (ch->chantype == RWMSG_CHAN_PEERSRV ||
              ch->chantype == RWMSG_CHAN_BROSRV) {
            rwmsg_broker_srvchan_t *sc = (rwmsg_broker_srvchan_t*)ch;
            RW_DL_REMOVE(&sc->methbindings, mb, elem);
          }

          memset(mb, 0, sizeof(*mb));
          //TBD adjust ep->methbind_nxt?
          
        }
      }
      ent = NULL;
      if (mct >= mmax) {
    break;
      }
    }
  }
  RWMSG_RG_UNLOCK();

  while (mct--) {
    rwmsg_method_release(ep, relmeth[mct]);
  }

  return TRUE;
}


/*
 * Srvchan calls this to register a method binding.  Second+ bindings
 * of a method to multiple srvchans is not yet supported.  Binding of
 * a path+method to srvchans in different endpoints/tasklets is
 * inherently unpossible and will trigger an assert.
 */
rw_status_t rwmsg_endpoint_add_channel_method_binding(rwmsg_endpoint_t *ep,
                                                      rwmsg_channel_t *ch,
                                                      enum rwmsg_methb_type_e btype,
                                                      rwmsg_method_t *method) {
  rwmsg_srvchan_t *check_sc = NULL;
  rw_status_t rs = RW_STATUS_FAILURE;

  RWMSG_EP_LOCK(ep);
  RW_ASSERT(ep->methbind_nxt >= 1);

  // TBD: lookup / sanity / second and further bindings / rebinding api?

  if (ep->methbind_nxt < RWMSG_METHBIND_MAX) {
    struct rwmsg_methbinding_s *mb = &ep->methbind[ep->methbind_nxt];
    memset(mb, 0, sizeof(*mb));

    mb->k.bindtype = btype;
    if (btype == RWMSG_METHB_TYPE_SRVCHAN) {
      mb->k.bro_instid = 0;
    }
    else {
      RW_ASSERT(btype == RWMSG_METHB_TYPE_BROSRVCHAN);
      mb->k.bro_instid = ep->bro_instid;
	    if (ch->chantype == RWMSG_CHAN_BROSRV) {
        mb->local_service = 1;
      } else {
        RW_ASSERT(ch->chantype == RWMSG_CHAN_PEERSRV);
      }
    }
    mb->k.pathhash = method->pathhash;
    mb->k.payt = method->sig->payt;
    mb->k.methno = method->sig->methno;

    mb->ep = ep;
    mb->meth = method;

    mb->srvchans[0].ch = ch;
    mb->srvchans_ct = 1;

    ck_ht_hash_t hash;

    RWMSG_RG_LOCK();
    {

      RWMSG_TRACE_CHAN(ch, DEBUG, "rwmsg_endpoint_add_channel_method_binding() adding phash=0x%lx payt %u methno %u (bro-instid %u btype %u) path '%s'",
          mb->k.pathhash, mb->k.payt, mb->k.methno, mb->k.bro_instid, mb->k.bindtype, method->path);

      ep->methbind_nxt++;
      ck_pr_inc_32(&method->refct);

      int r;
      ck_ht_entry_t hent;

      ck_ht_hash(&hash, &rwmsg_global.localdesttab, &mb->k, sizeof(mb->k));
      ck_ht_entry_set(&hent, hash, &mb->k, sizeof(mb->k), mb);
      r = ck_ht_put_spmc(&rwmsg_global.localdesttab, hash, &hent);
      if (r) {
        /* New entry, good! */
        rs = RW_STATUS_SUCCESS;
      } else {
        /* Already existed, and it's not us (we checked above). */
        // TBD, second srvchan, or duplicate binding?!
        rwmsg_method_release(ep, method);
        ep->methbind_nxt--;
        rs = RW_STATUS_SUCCESS;
      }
      if (ch->chantype == RWMSG_CHAN_PEERSRV ||
          ch->chantype == RWMSG_CHAN_BROSRV) {
        rwmsg_broker_srvchan_t *sc = (rwmsg_broker_srvchan_t*)ch;
        RW_DL_INSERT_HEAD(&sc->methbindings, mb, elem);
      }
    }
    RWMSG_RG_UNLOCK();

    if (btype == RWMSG_METHB_TYPE_SRVCHAN) {
      RW_ASSERT(ch == (rwmsg_channel_t*)(check_sc=(rwmsg_srvchan_t*)rwmsg_endpoint_find_channel(ep, RWMSG_METHB_TYPE_SRVCHAN, -1, mb->k.pathhash, mb->k.payt, mb->k.methno)));
    }
    else {
      RW_ASSERT(btype == RWMSG_METHB_TYPE_BROSRVCHAN);
    }
  } else {
    RW_ASSERT(ep->methbind_nxt < RWMSG_METHBIND_MAX);
  }
  RWMSG_EP_UNLOCK(ep);
  if (check_sc) {
    RW_ASSERT(btype == RWMSG_METHB_TYPE_SRVCHAN);
    _RWMSG_CH_DEBUG_(check_sc, "--");
    rwmsg_srvchan_release(check_sc);
  }
  return rs;
}

/*
 * Delete a path from the endpoint.  Actually refct based, path_add
 * and path_del calls must match up for the path to go away.
 */
rwmsg_bool_t rwmsg_endpoint_path_del(rwmsg_endpoint_t *ep,
                     uint32_t idx) {
  rwmsg_bool_t rval = FALSE;
  rwmsg_bool_t epderef=FALSE;
  RWMSG_EP_LOCK(ep);
  {
    if (idx == 0 || idx >= ep->paths_nxt) {
      goto bail;
    }
    struct rwmsg_path_s *pp = &ep->paths[idx];
    RW_ASSERT(pp->ep == ep);

    pp->refct--;
    if (!pp->refct) {
      /* SPMC hash.  As we hold the big RG lock, we are the single writer into this hash. */
      RWMSG_RG_LOCK();
      {
        RW_ASSERT(pp->refct == 0);

        int r;
        ck_ht_entry_t hent;
        ck_ht_hash_t hash;

        ck_ht_hash(&hash, &rwmsg_global.localpathtab, pp->path, strlen(pp->path));
        ck_ht_entry_set(&hent, hash, pp->path, strlen(pp->path), ep);
        r = ck_ht_remove_spmc(&rwmsg_global.localpathtab, hash, &hent);
        if (r) {
          /* Good */
        } else {
          RW_ASSERT(r);
        }
      }
      RWMSG_RG_UNLOCK();

      memset(pp, 0, sizeof(*pp));
      /* Fix paths_nxt etc */

      /* The hash entry was a reference to this endpoint */
      epderef = TRUE;
    }
    rval = TRUE;
  }
 bail:
  RWMSG_EP_UNLOCK(ep);
  if (epderef) {
    rwmsg_endpoint_release(ep);
  }
  return rval;
}

/* Normalize path and hash it */
uint64_t rwmsg_endpoint_path_hash(rwmsg_endpoint_t *ep,
                  const char *path) {
  char tmpath[RWMSG_PATH_MAX];
  tmpath[0]='\0';
  //sprintf(tmpath, "/SID%u", ep->sysid);
  int off = 0;//strlen(tmpath);
  strncat(tmpath+off, path, sizeof(tmpath)-1-off);
  return rw_hashlittle64(tmpath, strlen(tmpath));
}


/*
 * Add a path.  Used for local delivery clichan at first send.
 */
uint32_t rwmsg_endpoint_path_add(rwmsg_endpoint_t *ep,
                 const char *path,
                 uint64_t *pathhash_out) {

  uint32_t idx=0;

  size_t plen = strlen(path);
  if (plen+1+16/*SID#####*/ > RWMSG_PATH_MAX) {
    goto ret;
  }
  if (plen < 4) {
    RW_ASSERT(plen >= 4);
    goto ret;
  }
  if (path[0] != '/') {
    RW_ASSERT(path[0] == '/');
    goto ret;
  }
  if (0!=strncmp("/R/", path, 3) && 0!=strncmp("/L/", path, 3)) {
    RW_ASSERT(!strncmp("/R/", path, 3) || !strncmp("/L/", path, 3));
    goto ret;
  }
  const char *lastelem = strrchr(path, '/');
  uint32_t instance=0;
  if (lastelem) {
    lastelem++;
    if (!lastelem[0]) {
      /* Assign a number, really we need broker/zk assist to do this */
      RW_ASSERT(lastelem[0]);    /* unsupported */
      goto ret;
    } else {
      if (!isdigit(lastelem[0])) {
    RW_ASSERT(isdigit(lastelem[0]));
    goto ret;
      }
      char *endptr=NULL;
      instance = strtoul(lastelem, &endptr, 10);
      RW_ASSERT(endptr);
      if (endptr == lastelem) {
    RW_ASSERT(endptr != lastelem);
    goto ret;
      }
    }
  }

  /* Normalize path and hash it */
  char tmpath[RWMSG_PATH_MAX];
  tmpath[0]='\0';
  //!! ep may be null //sprintf(tmpath, "/SID%u", ep->sysid);
  int off = 0;//strlen(tmpath);
  strncat(tmpath+off, path, sizeof(tmpath)-1-off);
  uint64_t phash = rw_hashlittle64(tmpath, strlen(tmpath));
  if (pathhash_out) {
    *pathhash_out = phash;
  }

  RWMSG_EP_LOCK(ep);
  {
    uint32_t i;
    for (i=1; i<ep->paths_nxt; i++) {
      if(0==strcmp(tmpath, ep->paths[i].path)) {
    ep->paths[i].refct++;
    idx = i;
    RW_ASSERT(ep->paths[idx].pathhash == phash);
    goto bail;
      }
    }

    if (ep->paths_nxt >= (RWMSG_PATHTAB_MAX-1)) {
      RW_ASSERT(ep->paths_nxt < (RWMSG_PATHTAB_MAX-1));
      idx=0;
      goto bail;
    }

    idx = i;

    // assign this path at idx
    struct rwmsg_path_s *pp = &ep->paths[idx];
    memset(pp, 0, sizeof(*pp));
    ep->paths_nxt++;

    // fill in ep->paths[idx] including a ck_ht entry for it(self)

    pp->refct=1;
    RW_STATIC_ASSERT(sizeof(pp->path) == sizeof(tmpath));
    strncpy(pp->path, tmpath, sizeof(pp->path));
    pp->path[sizeof(pp->path)-1] = '\0';
    pp->ppfxlen = ((lastelem-1) - path) + off;
    pp->pathhash = phash;
    pp->ppfxhash = rw_hashlittle64(pp->path, pp->ppfxlen);
    pp->instance = instance;
    pp->ep = ep;

    /* SPMC hash.  As we hold the big RG lock, we are the single writer into this hash. */
    RWMSG_RG_LOCK();
    {
      int r;
      ck_ht_entry_t hent;
      ck_ht_hash_t hash;

      ck_ht_hash(&hash, &rwmsg_global.localpathtab, pp->path, strlen(pp->path));
      ck_ht_entry_set(&hent, hash, pp->path, strlen(pp->path), ep);
      r = ck_ht_put_spmc(&rwmsg_global.localpathtab, hash, &hent);
      if (r) {
        /* New entry, good! */
      } else {
        /* Already existed, and it's not us (we checked above). */
        RWMSG_TRACE(ep, ERROR, "Path add failed, another endpoint already registered path '%s'", path);
        ep->paths_nxt--;
        idx=0;
      }
    }
    RWMSG_RG_UNLOCK();

    /* The hash entry is a reference to this endpoint */
    if (idx) {
      ck_pr_inc_32(&ep->refct);
    }

  bail:
    RWMSG_EP_UNLOCK(ep);
  }

 ret:
  return idx;
}

void rwmsg_global_status_get(rwmsg_global_status_t *sout) {
  *sout = rwmsg_global.status;
}


static const char *rwmsg_objdesc[] = {
  "NULL",
  "endpoint",
  "method",
  "signature",
  "destination",
  "srvchan",
  "clichan",
  "rwsched_q",
  "rwsched_src",
  "sockset",
  "generic"
};

/*
 * Garbage handling.  Requires a timer tick, or better scheduler
 * support for quiescent state observation across all threads.
 * All items are queued only when refct reaches zero, and freed two
 * slow ticks later if the refct is still zero.
 */
static void rwmsg_garbage_truck_free(rwmsg_garbage_truck_t *gc,
                                     struct rwmsg_garbage_s *g) {
  RWMSG_TRACE(gc->ep, DEBUG, "GC free %s=%p ctx=%p tinfo=%p", rwmsg_objdesc[g->objtype], g->objptr, g->ctxptr, g->tinfo);

  switch (g->objtype) {
  case RWMSG_OBJTYPE_CLICHAN:
    rwmsg_clichan_destroy((rwmsg_clichan_t*)g->objptr);
    break;
  case RWMSG_OBJTYPE_SRVCHAN:
    rwmsg_srvchan_destroy((rwmsg_srvchan_t*)g->objptr);
    break;
  case RWMSG_OBJTYPE_RWSCHED_SRCREL:
    rwsched_dispatch_source_release((rwsched_tasklet_ptr_t)g->tinfo,
                                    (rwsched_dispatch_source_t)g->objptr);
    break;
  case RWMSG_OBJTYPE_RWSCHED_QREL:
    rwsched_dispatch_queue_release((rwsched_tasklet_ptr_t)g->tinfo,
                                   (rwsched_dispatch_queue_t)g->objptr);
    break;
  case RWMSG_OBJTYPE_METHOD:
    rwmsg_method_destroy(gc->ep, (rwmsg_method_t*)g->objptr);
    break;
  case RWMSG_OBJTYPE_SIGNATURE:
    rwmsg_signature_destroy(gc->ep, (rwmsg_signature_t*)g->objptr);
    break;
  case RWMSG_OBJTYPE_DESTINATION:
    rwmsg_destination_destroy((rwmsg_destination_t*)g->objptr);
    break;
  case RWMSG_OBJTYPE_SOCKSET:
    rwmsg_sockset_destroy((rwmsg_sockset_t *)g->objptr);
    break;
  case RWMSG_OBJTYPE_GENERIC:
    {
      void (*fp) (void *) = (void(*)(void*)) g->ctxptr;
    // WHAT TODO
      fp(g->objptr);
    }
    break;
  default:
    RW_CRASH();
    break;
  }

  if (g->dbgfile) {
    // from strdup
    free(g->dbgfile);
  }
  strcpy((char*)&(g->tinfo), "bad");
  RW_FREE_TYPE(g, struct rwmsg_garbage_s);
}

int rwmsg_garbage_truck_tick(rwmsg_garbage_truck_t *gc) {
  int ct = 0;
  RWMSG_EP_LOCK(gc->ep);

  gc->tick++;

  struct rwmsg_garbage_s *g;
  while ((g=RW_DL_HEAD(&gc->garbageq, struct rwmsg_garbage_s, elem))) {
    RW_ASSERT_TYPE(g, struct rwmsg_garbage_s);
    if (gc->tick <= g->tick) {
      break;
    }

    /* After two ticks it will be safe to delete items which had at
       defer time, and now still have, refct 0 */
    uint64_t age = gc->tick - g->tick;
    if (age < 2) {
      break;
    }
    struct rwmsg_garbage_s *g2 = RW_DL_REMOVE_HEAD(&gc->garbageq, struct rwmsg_garbage_s, elem);
    RW_ASSERT(g2 == g);
    RWMSG_EP_UNLOCK(gc->ep);
    //RWMSG_SLEEP_MS(50)
    rwmsg_garbage_truck_free(gc, g);
    RWMSG_EP_LOCK(gc->ep);
    ct++;
  }

  RWMSG_EP_UNLOCK(gc->ep);
  return ct;
}

void rwmsg_garbage_impl(rwmsg_garbage_truck_t *gc,
            enum rwmsg_garbage_type_e typ,
            void *objptr,
            void *ctxptr,
            void *tinfo,
            const char *dbgfile,
            int dbgline,
            void *dbgra) {
  RW_STATIC_ASSERT((sizeof(rwmsg_objdesc)/sizeof(char*)) == RWMSG_OBJTYPE_OBJCT);
  RW_ASSERT(typ < (sizeof(rwmsg_objdesc)/sizeof(char*)));

  //  RWMSG_TRACE(gc->ep, DEBUG, "GC defer %s=%p", rwmsg_objdesc[typ], objptr);

  RWMSG_EP_LOCK(gc->ep);

  /* This might be better in free to avoid high cost in this garbage call */
  struct rwmsg_garbage_s *in;
  for (in=RW_DL_HEAD(&gc->garbageq, struct rwmsg_garbage_s, elem);
       in;
       in=RW_DL_NEXT(in, struct rwmsg_garbage_s, elem)) {
    if (in->objptr == objptr) {
      /* Already in the queue */
      goto nope;
    }
  }

  struct rwmsg_garbage_s *g = (struct rwmsg_garbage_s*) RW_MALLOC0_TYPE(sizeof(*g), struct rwmsg_garbage_s);
  RW_ASSERT_TYPE(g, struct rwmsg_garbage_s);
  g->objtype = typ;
  g->objptr = objptr;
  g->ctxptr = ctxptr;
  g->tinfo = tinfo;
  g->tick = gc->tick;
  g->dbgfile = strdup(dbgfile);
  g->dbgline = dbgline;
  g->dbgra = dbgra;
  RW_DL_ENQUEUE(&gc->garbageq, g, elem);

 nope:
  RWMSG_EP_UNLOCK(gc->ep);
  return;
}

static void rwmsg_garbage_truck_rwtimer(void *gc) {
  rwmsg_garbage_truck_tick((rwmsg_garbage_truck_t*)gc);
}

void rwmsg_garbage_truck_init(rwmsg_garbage_truck_t *gc,
                  rwmsg_endpoint_t *ep) {
  memset(gc, 0, sizeof(*gc));
  gc->ep = ep;
  RW_DL_INIT(&gc->garbageq);
  gc->tick = time(NULL);

  /* Caution, dispatch timers don't block under cfrunloop.  Trouble
     from this should be unpossible, one shall not block while using
     reference counted objects without holding a reference.  */
  gc->rws_timer = rwsched_dispatch_source_create(ep->taskletinfo,
                                                 RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                 0,
                                                 0,
                                                 rwsched_dispatch_get_main_queue(ep->rwsched));
  uint64_t ival = 250/*ms*/ * 1000 * 1000;
  rwsched_dispatch_source_set_timer(gc->ep->taskletinfo, gc->rws_timer, DISPATCH_TIME_NOW, ival, ival/2);
  rwsched_dispatch_source_set_event_handler_f(gc->ep->taskletinfo, gc->rws_timer, rwmsg_garbage_truck_rwtimer);
  rwsched_dispatch_set_context(gc->ep->taskletinfo, gc->rws_timer, gc);
  rwsched_dispatch_resume(gc->ep->taskletinfo, gc->rws_timer);
}
