/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwmsg_api.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief API include for RW.Msg component
 */

#ifndef __RWMSG_API_H__
#define __RWMSG_API_H__

__BEGIN_DECLS

/* Note, rwmsg_status_t lives in rift-protobuf-c.h, ugly but true */

//?? Ought to be something in rwlib
typedef int rwmsg_bool_t;

typedef struct rwmsg_request_s rwmsg_request_t;
typedef struct rwmsg_srvchan_s rwmsg_srvchan_t;
typedef struct rwmsg_clichan_s rwmsg_clichan_t;

typedef struct rwmsg_destination_s rwmsg_destination_t;

typedef struct rwmsg_notify_s rwmsg_notify_t;
typedef struct rwmsg_sockset_s rwmsg_sockset_t;
typedef struct rwmsg_message_s rwmsg_message_t;
typedef struct rwmsg_buf_s rwmsg_buf_t;

typedef enum {
  RWMSG_BOUNCE_OK=0,
  RWMSG_BOUNCE_NODEST,
  RWMSG_BOUNCE_NOMETH,
  RWMSG_BOUNCE_NOPEER,
  RWMSG_BOUNCE_BROKERR,
  RWMSG_BOUNCE_TIMEOUT,
  RWMSG_BOUNCE_RESET,
  RWMSG_BOUNCE_SRVRST,
  RWMSG_BOUNCE_TERM,		/* server broker channel terminated */
  RWMSG_BOUNCE_BADREQ,

#define RWMSG_BOUNCE_CT (RWMSG_BOUNCE_TERM + 1)
} rwmsg_bounce_t;

//in milliseconds
#define RWMSG_BOUNCE_TIMEOUT_VAL (150 * NSEC_PER_SEC)


typedef enum { 
  RWMSG_PAYFMT_RAW=0,
  RWMSG_PAYFMT_PBAPI=1, 
  RWMSG_PAYFMT_TEXT=2,
  RWMSG_PAYFMT_MSGCTL=3,
  RWMSG_PAYFMT_AUDIT=4,
} rwmsg_payfmt_t;

//?? Meh.
#define RWMSG_PATH_MAX (512)

#include "rwmsg_endpoint.h"
#include "rwmsg_destination.h"
#include "rwmsg_signature.h"
#include "rwmsg_method.h"
#include "rwmsg_srvchan.h"
#include "rwmsg_clichan.h"
#include "rwmsg_request.h"


/* 
 * Broker functions to use when white box testing requires these functions in
 * another module
 */

void rwmsg_broker_test_main(uint32_t sid,
                            uint32_t instid,
                            uint32_t bro_instid,
                            rwsched_instance_ptr_t rws,
                            rwsched_tasklet_ptr_t tinfo,
                            void *rwcal,
                            uint32_t usemainq,
                            void **bro_out);

rwmsg_bool_t rwmsg_broker_test_halt(void *bro);

__END_DECLS


#endif // __RWMSG_API_H__

