
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
 * @file rwmsg_notify.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG event notification abstraction
 */

#include <sys/eventfd.h>

#include "rwmsg_int.h"

void rwmsg_notify_event(rwmsg_notify_t *notify, int pollbits) {

  uint64_t srcdata = 0;

  RW_ASSERT(notify);
  if (notify->halted) {
    return;
  }
  switch (notify->theme) {
  case RWMSG_NOTIFY_RWSCHED:
    {
      rwsched_dispatch_source_t src = notify->rwsched.rwsource;
      if (src) {
	srcdata = rwsched_dispatch_source_get_data(notify->rwsched.taskletinfo, src);
	RW_ASSERT(srcdata);	/* zero unpossible ?? */
	RWMSG_TRACE(notify->ep, DEBUG, "notify_event() notify=%p rwsched get_data() srcdata=%lu", notify, srcdata);
      }
    }
    break;
  default:
    break;
  }

  if (notify->cb.notify) {
    if (!notify->paused) {
      notify->cb.notify(notify->cb.ud, pollbits);
    }
  } else {
    /* Rwsched has to call clear 1:1 with raise calls, and the count
       of clear calls (presumed to be made from the callback) is an
       input to our merge_data() below.  If this doesn't happen
       precisely 1:1, we'll lose events or spin.  So, thou shalt not
       have a missing callback for the rwsched case. */
    RW_ASSERT(notify->theme != RWMSG_NOTIFY_RWSCHED);
  }

  ck_pr_barrier();

  switch (notify->theme) {
  case RWMSG_NOTIFY_RWSCHED:
    {
      rwsched_dispatch_source_t src = notify->rwsched.rwsource;
      if (src) {
	RWMSG_TRACE(notify->ep, DEBUG, "notify_event() notify=%p srcdata=%lu clearct=%u, %scalling _merge_data()", notify, srcdata, notify->rwsched.clearct, notify->rwsched.clearct > srcdata ? "" : "not ");
	if (srcdata > notify->rwsched.clearct) {
	  srcdata -= notify->rwsched.clearct;
	  notify->rwsched.clearct = 0;
	} else {
	  notify->rwsched.clearct -= srcdata;
	  srcdata = 0;
	}
	if (srcdata) {
	  rwsched_dispatch_source_merge_data(notify->rwsched.taskletinfo, src, srcdata);
	}
      }
    }
    break;
  default:
    break;
  }
}

/* From rwsched dispatch source */
static void rwmsg_notify_rwsched_event_r(rwmsg_notify_t *notify) {
  rwmsg_notify_event(notify, POLLIN);
}
static void rwmsg_notify_cancel_event(void *ctx) {
  rwmsg_notify_t *notify = (rwmsg_notify_t *)ctx;
  notify->cancelack = TRUE;
}

/* From rwsched_CFRunloop source */
static void rwmsg_notify_cfrunloop_callback(rwsched_CFSocketRef s,
					    CFSocketCallBackType type,
					    CFDataRef address,
					    const void *data,
					    void *info) {
  RW_ASSERT(info);
  //  rwmsg_notify_t *notify = (rwmsg_notify_t *)info;

  int pollbits = 0;
  if (type & kCFSocketDataCallBack) {
    pollbits |= POLLIN;
  } else if (type & kCFSocketReadCallBack) {
    pollbits |= POLLIN;
  } else if (type & kCFSocketAcceptCallBack) {
    pollbits |= POLLIN;
  }
  if (type & kCFSocketWriteCallBack) {
    pollbits |= POLLOUT;
  }
  rwmsg_notify_event(info, pollbits);

  return;
  s=s;
  address=address;
  data=data;
}

rw_status_t rwmsg_notify_init(rwmsg_endpoint_t *ep,
			      rwmsg_notify_t *notify,
			      const char *name,
			      rwmsg_closure_t *cb) {

  notify->ep = ep;

  if (name) {
    strncpy(notify->name, name, sizeof(notify->name)-1);
    notify->name[sizeof(notify->name)-1] = '\0';
  } else {
    memset(notify->name, 0, sizeof(notify->name));
  }
 
  rw_status_t rs = RW_STATUS_FAILURE;
  if (notify->theme == RWMSG_NOTIFY_INVALID) {
    notify->theme = RWMSG_NOTIFY_EVENTFD;
  }
  if (cb) {
    notify->cb = *cb;
  } else {
    memset(&notify->cb, 0, sizeof(notify->cb));
  }
  //  int setcan = FALSE;
  notify->halted = FALSE;
  notify->paused = FALSE;
  notify->wantack = FALSE;
  notify->cancelack = FALSE;
  notify->order = 0;
  switch (notify->theme) {
  case RWMSG_NOTIFY_EVENTFD:
    notify->fd = eventfd(0, (EFD_CLOEXEC|EFD_NONBLOCK|EFD_SEMAPHORE));
    if (notify->fd < 0) {
      goto ret;
    }
    ck_pr_inc_32(&ep->stat.objects.notify_eventfds);
    RW_ASSERT(notify->fd != 0); /* stdin?! */
    rs = RW_STATUS_SUCCESS;
    break;
  case RWMSG_NOTIFY_NONE:
    rs = RW_STATUS_SUCCESS;
    break;
  case RWMSG_NOTIFY_CFRUNLOOP:
    notify->fd = eventfd(0, (EFD_CLOEXEC|EFD_NONBLOCK|EFD_SEMAPHORE));
    if (notify->fd < 0) {
      goto ret;
    }
    ck_pr_inc_32(&ep->stat.objects.notify_eventfds);
    notify->order = 0;
    // fallthrough
  case RWMSG_NOTIFY_CFRUNLOOP_FD:
    RW_ASSERT(notify->fd != 0); /* stdin?! */
    RW_ASSERT(ep->rwsched);
    RW_ASSERT(notify->cfrunloop.taskletinfo);
    notify->cfrunloop.rwsched = ep->rwsched;

    {
      CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
      CFOptionFlags cf_callback_flags = 0;
      CFOptionFlags cf_option_flags = 0;
      rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(notify->cfrunloop.taskletinfo);
      rwsched_CFSocketRef cfsocket;
      rwsched_CFRunLoopSourceRef cfsource;
      cf_callback_flags |= kCFSocketReadCallBack;
      cf_option_flags |= kCFSocketAutomaticallyReenableReadCallBack;
      if (notify->theme == RWMSG_NOTIFY_CFRUNLOOP) {
	cf_option_flags |= kCFSocketCloseOnInvalidate;
      }
      
      cf_context.info = notify;
      cfsocket = rwsched_tasklet_CFSocketCreateWithNative(notify->cfrunloop.taskletinfo,
						  kCFAllocatorSystemDefault,
						  notify->fd,
						  cf_callback_flags,
						  rwmsg_notify_cfrunloop_callback,
						  &cf_context);
      RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);
      rwsched_tasklet_CFSocketSetSocketFlags(notify->cfrunloop.taskletinfo, cfsocket, cf_option_flags);
      cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(notify->cfrunloop.taskletinfo,
						     kCFAllocatorSystemDefault,
						     cfsocket,
						     notify->order);
      RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);
      rwsched_tasklet_CFRunLoopAddSource(notify->cfrunloop.taskletinfo, runloop, cfsource, RWMSG_RUNLOOP_MODE);

      notify->cfrunloop.cfsocket = cfsocket;
      notify->cfrunloop.cfsource = cfsource;
      rs = RW_STATUS_SUCCESS;
    }
    break;
  case RWMSG_NOTIFY_RWSCHED:
  case RWMSG_NOTIFY_RWSCHED_FD:
    RW_ASSERT(ep->rwsched);
    if (notify->theme == RWMSG_NOTIFY_RWSCHED) {
      RW_ASSERT(notify->rwsched.clearct == 0);
      notify->rwsched.rwsource = rwsched_dispatch_source_create(ep->taskletinfo,
                                                                RWSCHED_DISPATCH_SOURCE_TYPE_DATA_ADD,
                                                                0,
                                                                0,
                                                                notify->rwsched.rwqueue);
    } else {
      notify->rwsched.rwsource = rwsched_dispatch_source_create(ep->taskletinfo,
                                                                RWSCHED_DISPATCH_SOURCE_TYPE_READ,
                                                                notify->fd,
                                                                0,
                                                                notify->rwsched.rwqueue);
    }
    if (notify->rwsched.rwsource) {
      notify->rwsched.rwsched = ep->rwsched;
      notify->rwsched.taskletinfo = ep->taskletinfo;
      rwsched_dispatch_set_context(ep->taskletinfo, notify->rwsched.rwsource, notify);

      rwsched_dispatch_source_set_event_handler_f(ep->taskletinfo,
                                                  notify->rwsched.rwsource,
                                                  (void(*)(void*))rwmsg_notify_rwsched_event_r);
      rwsched_dispatch_resume(ep->taskletinfo, notify->rwsched.rwsource);
      rs = RW_STATUS_SUCCESS;
    }
    break;
  default:
    RW_CRASH();
    break;
  }
 ret:

  return rs;

  ep=ep;
}

void rwmsg_notify_pause(rwmsg_endpoint_t *ep,
			rwmsg_notify_t *notify) {
  if (!notify->paused && !notify->halted) {
    switch (notify->theme) {
    case RWMSG_NOTIFY_CFRUNLOOP:
    case RWMSG_NOTIFY_CFRUNLOOP_FD:
      if (notify->cfrunloop.cfsource) {
	notify->paused = TRUE;
	rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(notify->cfrunloop.taskletinfo);
	rwsched_tasklet_CFRunLoopRemoveSource(notify->cfrunloop.taskletinfo,
				      runloop,
				      notify->cfrunloop.cfsource,
				      RWMSG_RUNLOOP_MODE);
      }
      break;
    case RWMSG_NOTIFY_RWSCHED:
    case RWMSG_NOTIFY_RWSCHED_FD:
      if (notify->rwsched.rwsource) {
	notify->paused = TRUE;
	rwsched_dispatch_suspend(ep->taskletinfo, notify->rwsched.rwsource);
      }
      break;
    case RWMSG_NOTIFY_EVENTFD:
      // we just return no pollfds from getpollset
      notify->paused = TRUE;
      break;
    default:
      break;
    }
  }
}
void rwmsg_notify_resume(rwmsg_endpoint_t *ep,
			 rwmsg_notify_t *notify) {
  if (notify->paused && !notify->halted) {
    switch (notify->theme) {
    case RWMSG_NOTIFY_CFRUNLOOP:
    case RWMSG_NOTIFY_CFRUNLOOP_FD:
      if (notify->cfrunloop.cfsource) {
	notify->paused = FALSE;
	rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(notify->cfrunloop.taskletinfo);
	rwsched_tasklet_CFRunLoopAddSource(notify->cfrunloop.taskletinfo,
				   runloop,
				   notify->cfrunloop.cfsource,
				   RWMSG_RUNLOOP_MODE);
      }
      break;
    case RWMSG_NOTIFY_RWSCHED:
    case RWMSG_NOTIFY_RWSCHED_FD:
      if (notify->rwsched.rwsource) {
	notify->paused = FALSE;
	rwsched_dispatch_resume(ep->taskletinfo, notify->rwsched.rwsource);
      }
      break;
    case RWMSG_NOTIFY_EVENTFD:
      notify->paused = FALSE;
      break;
    default:
      break;
    }
  }
}

void rwmsg_notify_deinit(rwmsg_endpoint_t *ep,
			 rwmsg_notify_t *notify) {
  if (notify->halted) {
    return;
  }
  int canack = TRUE;
  notify->cancelack = FALSE;
  notify->halted = TRUE;	/* bit of a race here, need to deinit from our own rwsched queue */
  enum rwmsg_notify_theme_e theme = notify->theme;
  switch (theme) {
  case RWMSG_NOTIFY_CFRUNLOOP:
    if (notify->cfrunloop.cfsource) {
      ck_pr_dec_32(&ep->stat.objects.notify_eventfds); /* eventfd closed by CFSocketCloseOnInvalidate */
    }
  case RWMSG_NOTIFY_CFRUNLOOP_FD:
    if (notify->cfrunloop.cfsource) {
      rwsched_tasklet_CFSocketInvalidate(notify->cfrunloop.taskletinfo, notify->cfrunloop.cfsocket);
      // for CFRUNLOOP, eventfd is closed per cfsock flag kCFSocketCloseOnInvalidate
      // for CFRUNLOOP_FD, it's the caller's problem
      memset(&notify->cfrunloop, 0, sizeof(notify->cfrunloop));
    }
    break;
  case RWMSG_NOTIFY_RWSCHED:
  case RWMSG_NOTIFY_RWSCHED_FD:
    if (notify->rwsched.rwsource) {
      RW_ASSERT_TYPE(notify->rwsched.rwsource, rwsched_dispatch_source_t);
      int waspaused=FALSE;
      if (notify->paused) {
	waspaused = TRUE;
	rwsched_dispatch_resume(ep->taskletinfo, notify->rwsched.rwsource);
	//	rwmsg_notify_resume(ep, notify);
	//	RW_ASSERT(!notify->paused);
      }
      rwsched_dispatch_source_t rwsrc = notify->rwsched.rwsource;
      notify->rwsched.rwsource = NULL;
      RW_ASSERT(ep->rwsched == notify->rwsched.rwsched);
      if (notify->wantack) {
	//RWMSG_TRACE(ep, DEBUG, "setting cancel_handler_f on rwsrc=%p in notify=%p", rwsrc, notify);
	rwsched_dispatch_source_set_cancel_handler_f(ep->taskletinfo, 
						     rwsrc,
						     rwmsg_notify_cancel_event);
      }

      /*
      RWMSG_TRACE(ep, DEBUG, "notify=%p '%s' calling cancel, .wantack=%d, rwsrc=%p rwq=%p (mainq=%p)", 
		  notify, notify->name, notify->wantack,
		  rwsrc,
		  notify->rwsched.rwqueue,
		  rwsched_dispatch_get_main_queue(ep->rwsched));
      */
      rwsched_dispatch_source_cancel(ep->taskletinfo, rwsrc);

      if (notify->wantack) {
	canack = FALSE;
      } else {
	usleep(25000);		/* fugly, need _cancel_sync!!! */
      }

      waspaused=waspaused;

      rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, rwsrc, ep->rwsched, ep->taskletinfo);
      memset(&notify->rwsched, 0, sizeof(notify->rwsched));
    }
    break;
  case RWMSG_NOTIFY_EVENTFD:
    if (notify->fd >= 0) {
      close(notify->fd);
      notify->fd = -1;
      ck_pr_dec_32(&ep->stat.objects.notify_eventfds);
    }
    break;
  case RWMSG_NOTIFY_NONE:
  case RWMSG_NOTIFY_INVALID:
    break;
  default:
    RW_CRASH();
    break;
  }
  notify->theme = RWMSG_NOTIFY_INVALID;
  if (canack) {
    //RWMSG_TRACE(ep, DEBUG, "notify=%p deinit explicit canack setting", notify);
    notify->cancelack = TRUE;
  } else {
    RW_ASSERT(theme == RWMSG_NOTIFY_RWSCHED || theme == RWMSG_NOTIFY_RWSCHED_FD);
    //RWMSG_TRACE(ep, DEBUG, "notify=%p deinit deferred canack setting", notify);
  }
  return;
}

int rwmsg_notify_getpollset(rwmsg_notify_t *notify,
			    struct pollfd *pollset,
			    int pollset_len) {
  int retval = 0;
  switch (notify->theme) {
  case RWMSG_NOTIFY_NONE:
    break;
  default:
    RW_CRASH();
    break;
  case RWMSG_NOTIFY_EVENTFD:
    RW_ASSERT(pollset_len >= 1);
    if (notify->paused) {
      break;
    }
    pollset[0].fd = notify->fd;
    pollset[0].events = POLLIN;
    pollset[0].revents = 0;
    retval = 1;
    break;
  }
  return retval;
}

void rwmsg_notify_raise(rwmsg_notify_t *notify) {
  switch (notify->theme) {
  case RWMSG_NOTIFY_EVENTFD:
    if (notify->fd >= 0) {
      uint64_t val = 1;
      errno = 0;
      int r;
      r = write(notify->fd, &val, sizeof(val));
      if (r < 0) {
	//	perror("rwmsg_notify_raise(EV)");
      } else {
	notify->eventfd.dbgval += val;
      }
    }
    break;
  case RWMSG_NOTIFY_CFRUNLOOP:
    if (notify->fd >= 0) {
      uint64_t val = 1;
      errno = 0;
      int r;
      r = write(notify->fd, &val, sizeof(val));
      if (r < 0) {
	//	perror("rwmsg_notify_raise(CF)");
      } else {
	notify->eventfd.dbgval += val;
      }
    }
    break;
  case RWMSG_NOTIFY_RWSCHED:
    if (notify->rwsched.rwsource) {
      RWMSG_TRACE(notify->ep, DEBUG, "notify_raise() notify=%p", notify);
      rwsched_dispatch_source_merge_data(notify->rwsched.taskletinfo, notify->rwsched.rwsource, 1);
    }
    break;
  case RWMSG_NOTIFY_RWSCHED_FD:
  case RWMSG_NOTIFY_CFRUNLOOP_FD:
    rwmsg_notify_event(notify, POLLIN);
    break;
  case RWMSG_NOTIFY_NONE:
    break;
  default:
    RW_CRASH();
    break;
  }
}

void rwmsg_notify_clear(rwmsg_notify_t *notify, uint32_t ct) {
  switch (notify->theme) {
  case RWMSG_NOTIFY_EVENTFD:
    {
      while (ct--) {
	uint64_t val=0;
	int r = read(notify->fd, &val, sizeof(val));
	if (r < 8) {
	  if (r<0) {
	    //perror("rwmsg_notify_clear(EV) read=-1");
	  }
	  break;
	} else {
	  notify->eventfd.dbgval--;
	}
      }
    }
    break;
  case RWMSG_NOTIFY_CFRUNLOOP:
    {
      while (ct--) {
	uint64_t val=0;
	int r = read(notify->fd, &val, sizeof(val));
	if (r < 8) {
	  if (r<0) {
	    //perror("rwmsg_notify_clear(CF) read=-1");
	  }
	  break;
	} else {
	  notify->cfrunloop.dbgval--;
	}
      }
    }
    break;
  case RWMSG_NOTIFY_CFRUNLOOP_FD:
  case RWMSG_NOTIFY_RWSCHED_FD:
  case RWMSG_NOTIFY_NONE:
    break;
  case RWMSG_NOTIFY_RWSCHED:
    RWMSG_TRACE(notify->ep, DEBUG, "notify_clear() notify=%p .clearct=%u, clear(ct=%u)", notify, notify->rwsched.clearct, ct);
    notify->rwsched.clearct += ct;
    break;
  default:
    RW_CRASH();
    break;
  }
}
