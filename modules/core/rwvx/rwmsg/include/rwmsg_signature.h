
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
 * @file rwmsg_signature.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Signature include for RW.Msg component
 */

#ifndef __RWMSG_SIGNATURE_H__
#define __RWMSG_SIGNATURE_H__

__BEGIN_DECLS


/*
 *  Signature object
 */

typedef struct rwmsg_signature_s rwmsg_signature_t;

typedef enum {
  RWMSG_PRIORITY_LOW=0,
  RWMSG_PRIORITY_MEDIUM=1,
  RWMSG_PRIORITY_HIGH=2,
  RWMSG_PRIORITY_PLATFORM=3,
  RWMSG_PRIORITY_BLOCKING=4,	/* special cased, responses only */
  RWMSG_PRIORITY_COUNT
#define RWMSG_PRIORITY_DEFAULT (RWMSG_PRIORITY_LOW)
} rwmsg_priority_t;


/**
 * Create a request signature.  This encapsulates the payload type,
 * on-wire method number, request priority, and a few other
 * attributes.  A signature may be used for both method registrations
 * and for outgoing requests.  Signatures may be shared among threads.
 * @param ep The tasklet's endpoint.
 * @param payfmt The payload format for this signature.  The format defines the meaning of the method numbering.
 * @param pri The message priority for this signature.
 * @return A handle on the signature.
 */
rwmsg_signature_t *rwmsg_signature_create(rwmsg_endpoint_t *ep,
					  rwmsg_payfmt_t payt,
					  uint32_t method,
					  rwmsg_priority_t pri);
/**
 * Release a signature.  If this is the last reference to the object, it will be freed.
 * @param ep The tasklet's endpoint.
 * @param sig The signature to release.
 */
void rwmsg_signature_release(rwmsg_endpoint_t *ep,
			     rwmsg_signature_t *sig);

#if 0
//TBD --gads no-- !!!
void rwmsg_signature_set_reentrant(rwmsg_signature_t *sig, 
                                   rwmsg_bool_t on);
#endif

void rwmsg_signature_set_timeout(rwmsg_signature_t *sig,
				 uint32_t ms);

typedef struct rwmsg_signature_stats_s {
  uint64_t reqs_in;
  uint64_t rsps_out;
  //...
} rwmsg_signature_stats_t;

void rwmsg_signature_get_stats(rwmsg_signature_t *sig, 
                               rwmsg_signature_stats_t *stats_out);

__END_DECLS

#endif // __RWMSG_SIGNATURE_H

