
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */


#ifndef __RWMSG_GTEST_C_H
#define __RWMSG_GTEST_C_H

__BEGIN_DECLS

int rwmsg_gtest_queue_create_delete(rwmsg_endpoint_t *ep);
int rwmsg_gtest_queue_queue_dequeue(rwmsg_endpoint_t *ep);
int rwmsg_gtest_queue_pollable_event(rwmsg_endpoint_t *ep);
int rwmsg_gtest_srvq(rwmsg_srvchan_t *sc);

__END_DECLS

#endif // __RWMSG_GTEST_C_H
