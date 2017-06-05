/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwmsg_method.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Method include for RW.Msg component
 *
 * @details
 * The rwmsg_method_t object binds method signatures to destinations
 * in the system.  It is used primarily to register callbacks on an
 * rwmsg_srvchan_t for message servicing routines.
 */

#ifndef __RWMSG_METHOD_H__
#define __RWMSG_METHOD_H__

__BEGIN_DECLS

/*
 *  Method object
 */

typedef struct rwmsg_method_s rwmsg_method_t;

/**
 * Create a method object.  The method object binds a signature to a
 * nameservice path; once the method is bound to a srvchannel requests
 * will arrive via the given callback.
 *
 * @param ep The endpoint in which to create this method.
 * @param path The path for this method.  Copied.
 * @param sig The signature for this method.  Referenced with refct.
 * @param requestcb The callback for this method.  Copied.
 * @return The method created.
 */
rwmsg_method_t *rwmsg_method_create(rwmsg_endpoint_t *ep,
				    const char *path,
				    rwmsg_signature_t *sig,
				    rwmsg_closure_t *requestcb);
/**
 * Release a reference to a method object.  When there are no further references the method will be freed.
 * @param ep The endpoint in which this method lives.
 * @param meth The method to release.
 */
void rwmsg_method_release(rwmsg_endpoint_t *ep,
			  rwmsg_method_t *meth);



#if 0
void rwmsg_method_set_callback(rwmsg_method_t *meth,
                               rwmsg_closure_t *requestcb);
void rwmsg_method_tcp_accept(rwmsg_method_t *meth,
                             rwmsg_closure_t *acceptcb);
#endif

__END_DECLS

#endif // __ RWMSG_METHOD_H
