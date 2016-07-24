
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_queue.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Internal use only -- notification mechanism for rwmsg runtime interfacing
 */

#ifndef __RWMSG_NOTIFY_H__
#define __RWMSG_NOTIFY_H__

#include <poll.h>

__BEGIN_DECLS

/* Notification interface, internal.  The notify needs to be set up
   ahead of time and passed in, depending on the theme a cb may be
   required, or for some notification themes (poll/eventfd, null) the
   caller needs to drive by calling rwmsg_notify_event(rwmsg_notify_t
   *notify) when events happen. */

rw_status_t rwmsg_notify_init(rwmsg_endpoint_t *ep,
			      rwmsg_notify_t *notify,
			      const char *name,
			      rwmsg_closure_t *cb);
void rwmsg_notify_deinit(rwmsg_endpoint_t *ep,
			 rwmsg_notify_t *notify);

void rwmsg_notify_raise(rwmsg_notify_t *notify);
void rwmsg_notify_clear(rwmsg_notify_t *notify, uint32_t ct);

int rwmsg_notify_getpollset(rwmsg_notify_t *notify,
			    struct pollfd *pollset_out,
			    int pollset_max);

void rwmsg_notify_pause(rwmsg_endpoint_t *ep,
			rwmsg_notify_t *notify);
void rwmsg_notify_resume(rwmsg_endpoint_t *ep,
			 rwmsg_notify_t *notify);

__END_DECLS

#endif // __RWMSG_NOTIFY_H__
