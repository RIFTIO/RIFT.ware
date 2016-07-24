
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_destination.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Destination include for RW.Msg component
 */

#ifndef __RWMSG_DESTINATION_H__
#define __RWMSG_DESTINATION_H__

__BEGIN_DECLS

/*
 *   Destination object
 */

typedef enum rwmsg_addrtype_e {
  RWMSG_ADDRTYPE_UNICAST,
  RWMSG_ADDRTYPE_ANYCAST,
  RWMSG_ADDRTYPE_MULTICAST
} rwmsg_addrtype_t;

typedef uint32_t rwmsg_flowid_t;

/** 
 * Create a destination object, used as an input to
 * rwmsg_clichan_send().  At present only unicast addresses are
 * supported.  Each different rwsched context (cfrunloop or serial
 * queue) with bound clichans should have a separate
 * rwmsg_destination_t object; for simplicity it is probably best to
 * create dedicated rwmsg_destination_t objects for each clichan.
 *
 * There are no particular restrictions on srvchan method bindings
 * sharing rwmsg_destination_t objects at this time.
 */
rwmsg_destination_t *rwmsg_destination_create(rwmsg_endpoint_t *ep,
					      rwmsg_addrtype_t atype, 
					      const char *addrpath);

/**
 * API to set no-connection-dont-Q flag inside rwmsg_destination_t
 * With this set if the connection to the broker is not connected, the
 * send() fails with RW_STATUS_NOTCONNECTED
 */
void rwmsg_destination_set_noconndontq(rwmsg_destination_t *dt);

/**
 * API to unset no-connection-dont-Q flag inside rwmsg_destination_t
 */
void rwmsg_destination_unset_noconndontq(rwmsg_destination_t *dt);

/**
 * 
 * Destroys the destination object.
 */
void rwmsg_destination_destroy(rwmsg_destination_t *dt);

/**
 * Release a reference to a destination, and free the destination
 * object if this was the last reference.
 */
void rwmsg_destination_release(rwmsg_destination_t *dest);
rw_status_t rwmsg_destination_get_count(rwmsg_destination_t *dest, int *ct_out);
int rwmsg_destination_get_outstanding(rwmsg_destination_t *dest);

typedef struct rwmsg_destination_stats_s {
  uint64_t out;
  uint64_t in;
} rwmsg_destination_stats_t;
void rwmsg_destination_get_stats(rwmsg_destination_stats_t *stats_out);

struct rwmsg_closure_s {
  /* must match the skeletal decl of rwmsg_closure_s up in rwmsg_api.h */
  union {
    void (*request)(rwmsg_request_t *req, void *ud);
    void (*response)(rwmsg_request_t *req, void *ud);
    void (*connect)(int fd, int errno, void *ud);
    void (*accept)(int fd, void *ud);
    //...
    void (*notify)(void *ud, int bits);
    void (*pbrsp)(ProtobufCMessage *pcmsg, rwmsg_request_t *req, void *ud);
    void (*feedme)(void *ud, rwmsg_destination_t *dest, rwmsg_flowid_t srvflowid);
    void (*qfeed)(void *ud);
  };
  void *ud;

  // blocks?
};
typedef struct rwmsg_closure_s rwmsg_closure_t;

/** 
 * Set a feedme callback on the default client flow present in this
 * destination handle.  This flow will be used for requests sent into
 * a clichan before the destination-signature has been assigned a flow
 * by the destination srvchan (typically this would be just the first
 * request).  See rwmsg_clichan_feedme_callback() for more detail.
 *
 * Unlike rwmsg_clichan_feedme_callback(),
 * rwmsg_destination_feedme_callback() can be called before binding
 * the clichan to a callback and from any scheduler context.
 */
void rwmsg_destination_feedme_callback(rwmsg_destination_t *dest, rwmsg_closure_t *cb);

rw_status_t rwmsg_destination_connect(int timeout, rwmsg_closure_t *cb);

__END_DECLS

#endif  // __RWMSG_DESTINATION_H__
