
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_sockset.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Internal API include for RW.Msg component
 */

#ifndef __RWMSG_SOCKSET_H__
#define __RWMSG_SOCKSET_H__

__BEGIN_DECLS

/*
 * Socket set object (internal), our connection to the broker.  Also
 * used by the broker.  This provides a) a bundle of prioritized nn
 * sockets and b) glue to rwsched/cfrunloop/null event sources.
 */

rwmsg_sockset_t *rwmsg_sockset_create(rwmsg_endpoint_t *ep, uint8_t *hsval, size_t hsval_sz, rwmsg_bool_t ageout);

struct rwmsg_sockset_closure_s {
  void (*recvme)(rwmsg_sockset_t *sk, rwmsg_priority_t pri, void *ud);
  void (*sendme)(rwmsg_sockset_t *sk, rwmsg_priority_t pri, void *ud);
  void *ud;
};
typedef struct rwmsg_sockset_closure_s rwmsg_sockset_closure_t;

rw_status_t rwmsg_sockset_bind_rwsched_queue(rwmsg_sockset_t *sk, rwsched_dispatch_queue_t q, rwmsg_sockset_closure_t *cb);
rw_status_t rwmsg_sockset_bind_rwsched_cfrunloop(rwmsg_sockset_t *sk, rwsched_tasklet_ptr_t tinfo, rwmsg_sockset_closure_t *cb);

rw_status_t rwmsg_sockset_connect(rwmsg_sockset_t *sk, const char *nnuri, int ct);
rw_status_t rwmsg_sockset_connect_pri(rwmsg_sockset_t *ss, const char *uri, rwmsg_priority_t pri);

/* Add this fd at the given pri; for broker to accept incoming connections */
rw_status_t rwmsg_sockset_attach_fd(rwmsg_sockset_t *sk, int fd, rwmsg_priority_t pri);

rw_status_t rwmsg_sockset_send(rwmsg_sockset_t *sk, rwmsg_priority_t pri, rwmsg_buf_t *buf);
rw_status_t rwmsg_sockset_send_copy(rwmsg_sockset_t *sk, rwmsg_priority_t pri, rwmsg_buf_t *buf);
rw_status_t rwmsg_sockset_recv_pri(rwmsg_sockset_t *sk, rwmsg_priority_t pri, rwmsg_buf_t *buf);
rw_status_t rwmsg_sockset_recv(rwmsg_sockset_t *sk, rwmsg_buf_t *buf, rwmsg_priority_t *pri_out);

/* Clean shutdown on all sockets, also releases, GC then destroys */
void rwmsg_sockset_close(rwmsg_sockset_t *sk);
void rwmsg_sockset_release(rwmsg_sockset_t *sk);
void rwmsg_sockset_destroy(rwmsg_sockset_t *sk);

rwsched_CFRunLoopSourceRef rwmsg_sockset_get_blk_cfsrc(rwmsg_sockset_t *ss);

/// TBD:

/* Flow control; pause pauses pri AND BELOW; resume resumes pri and below until the first pri with pollout==1 */
void rwmsg_sockset_pause_pri(rwmsg_sockset_t *sk, rwmsg_priority_t pri);
void rwmsg_sockset_resume_pri(rwmsg_sockset_t *sk, rwmsg_priority_t pri);
rwmsg_bool_t rwmsg_sockset_paused_p(rwmsg_sockset_t *sk, rwmsg_priority_t pri);

void rwmsg_sockset_pollout(rwmsg_sockset_t *sk, rwmsg_priority_t pri, rwmsg_bool_t onoff);

rw_status_t rwmsg_sockset_connection_status(rwmsg_sockset_t *ss);

//??rwmsg_bool_t rwmsg_sockset_connected_p(rwmsg_sockset_t *sk, rwmsg_priority_t pri);

__END_DECLS


#endif // __RWMSG_SOCKSET_H__

