
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_endpoint.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Endpoint include for RW.Msg component
 */

#ifndef __RWMSG_ENDPOINT_H__
#define __RWMSG_ENDPOINT_H__

__BEGIN_DECLS

/*
 * Endpoint object
 */

typedef struct rwmsg_endpoint_s rwmsg_endpoint_t;

/* In yang-emitted manifest declarations from rwvcs */
struct RwManifest__YangData__RwManifest__Manifest__InitPhase__Settings__Rwmsg;

/**
 * Create the one endpoint for this tasklet.  Each tasklet has one
 * endpoint used as the primary handle to many rwmsg calls.
 * @return Handle to the endpoint.
 */
rwmsg_endpoint_t *rwmsg_endpoint_create(uint32_t sysid,
					uint32_t inst_id,
					uint32_t bro_instid,
					rwsched_instance_ptr_t rws,
					rwsched_tasklet_ptr_t tinfo,
					rwtrace_ctx_t *rwctx,
					struct RwManifest__YangData__RwManifest__Manifest__InitPhase__Settings__Rwmsg *manifest_rwmsg);

/**
 * Halt the endpoint.  This includes assorted one-time cleanup, and an
 * implicit release call.  Call it once per create.
 *
 * @return TRUE if the refct happened to hit zero
 */
rwmsg_bool_t rwmsg_endpoint_halt(rwmsg_endpoint_t *ep);

/**
 * Halt the endpoint, including running extra voodoo GC cycles.  This
 * includes assorted one-time cleanup, and an implicit release call.
 * Call it once per create, probably only in peculiar unit test
 * situations.
 *
 * @return TRUE if the refct happened to hit zero
 */
rwmsg_bool_t rwmsg_endpoint_halt_flush(rwmsg_endpoint_t *ep, int flush);

/**
 * Unreference the one endpoint for this tasklet, delete if refct hits
 * zero.  Probably not useful in app code at this time.
 *
 * @return TRUE if the refct happened to hit zero
 */
rwmsg_bool_t rwmsg_endpoint_release(rwmsg_endpoint_t *ep);

/**
 * Get a property.  These are globally propogated through the broker(s)/zk/shm/magic.
 * @param ep The endpoint handle
 * @param ppath String path of the property to get
 * @param i_in Integer value to get.
 */
rw_status_t rwmsg_endpoint_get_property_int(rwmsg_endpoint_t *ep, 
					    const char *ppath, 
					    int *i_out);
/**
 * Get a property.  These are globally propogated through the broker(s)/zk/shm/magic.
 * @param ep The endpoint handle
 * @param ppath String path of the property to get
 * @param str_out Buffer to copy string value into
 * @param str_out_len Size of output buffer.  Value will be zero terminated and TRUNCATED TO FIT.
 */
rw_status_t rwmsg_endpoint_get_property_string(rwmsg_endpoint_t *ep, 
					       const char *ppath, 
					       char *str_out, 
					       int str_out_len);

/**
 * Set a property.  These are globally propogated through the broker(s)/zk/shm/magic.
 * @param ep The endpoint handle
 * @param ppath String path of the property to set
 * @param i_in Integer value to set.
 */
void rwmsg_endpoint_set_property_int(rwmsg_endpoint_t *ep, 
                                     const char *ppath, 
                                     int32_t i_in);
/**
 * Set a property.  These are globally propogated through the broker(s)/zk/shm/magic.
 * @param ep The endpoint handle
 * @param ppath String path of the property to set
 * @param str_in String value to set.
 */
void rwmsg_endpoint_set_property_string(rwmsg_endpoint_t *ep, 
                                        const char *ppath, 
                                        const char *str_in);



rwsched_instance_ptr_t rwmsg_endpoint_get_rwsched(rwmsg_endpoint_t *ep);

enum rwmsg_broker_state {
  RWMSG_BROKER_STATE_NULL=0,
  RWMSG_BROKER_STATE_CONNECTED=1,
  //...
};

struct rwmsg_queue_stats_s {
  uint32_t foo;
  //...
};

typedef struct rwmsg_global_status_s {
  uint32_t endpoint_ct;
  uint32_t qent;
  uint32_t request_ct;
} rwmsg_global_status_t;

/**
 * Return a copy of a structure containing rwmsg process-global status detail.
 */
void rwmsg_global_status_get(rwmsg_global_status_t *sout);

typedef struct rwmsg_endpoint_status_s {
  struct {
    uint32_t destinations;
    uint32_t methods;
    uint32_t signatures;
    uint32_t requests;
    uint32_t queues;
    uint32_t srvchans;
    uint32_t clichans;
    uint32_t brosrvchans;
    uint32_t broclichans;
    uint32_t peersrvchans;
    uint32_t peerclichans;
    uint32_t socksets;
    uint32_t nn_eventfds;
    uint32_t notify_eventfds;
  } objects;

  struct rwmsg_endpoint_status_detail_s {
    enum {
      RWMSG_ENDPOINT_OBJ_TYPE
    } objtype;
#define RWMSG_ENDPOINT_OBJ_NAME_MAX (64)
    char objname[RWMSG_ENDPOINT_OBJ_NAME_MAX];
    union {
      struct rwmsg_queue_stats_s queue;
      //...
    };
  } *detail;
  uint32_t detail_len;

  //...
} rwmsg_endpoint_status_t;

/**
 * Trace nonzero object counts in this endpoint's status.  For gtests. 
 */
void rwmsg_endpoint_trace_status(rwmsg_endpoint_t *s);

/**
 * Free the runtime status object returned by rwmsg_endpoint_get_status_FREEME().
 */
void rwmsg_endpoint_get_status_free(rwmsg_endpoint_status_t *status_trash);

/**
 * Return assorted runtime status data about the endpoint.
 * @param status_out Pointer to pointer to status structure to be
 * filled in.  Do not forget to free the returned value with
 * rwmsg_endpoint_get_status_free().
 */
void rwmsg_endpoint_get_status_FREEME(rwmsg_endpoint_t *ep,
				      rwmsg_endpoint_status_t **status_out);


__END_DECLS


#endif // __RWMSG_ENDPOINT_H__

