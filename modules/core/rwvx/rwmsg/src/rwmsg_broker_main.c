
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_broker_main.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG broker
 */

#include "rwmsg_int.h"
#include "rwmsg_broker.h"

void rwmsg_broker_main(uint32_t sid,
		       uint32_t instid,
		       uint32_t bro_instid,
		       rwsched_instance_ptr_t rws,
		       rwsched_tasklet_ptr_t tinfo,
		       rwcal_module_ptr_t rwcal,
		       uint32_t usemainq,
           rwtasklet_info_t *rwtasklet_info,
		       rwmsg_broker_t **bro_out) {
  
  rwmsg_broker_t *bro;
  rwmsg_endpoint_t *ep;

  ep = rwmsg_endpoint_create(sid, instid, bro_instid, rws, tinfo, rwtrace_init(), NULL);

  bro = rwmsg_broker_create(sid, bro_instid, NULL, rws, tinfo, rwcal, usemainq, ep, rwtasklet_info);
  if (bro_out) {
    *bro_out = bro;
  }

  /* Decrement by one, such that broker_halt will release the endpoint */
  RW_ASSERT(ep->refct >= 2);
  rwmsg_endpoint_release(ep);

  return;
}

void rwmsg_broker_test_main(uint32_t sid,
                            uint32_t instid,
                            uint32_t bro_instid,
                            rwsched_instance_ptr_t rws,
                            rwsched_tasklet_ptr_t tinfo,
                            void *rwcal,
                            uint32_t usemainq,
                            void **bro_out) {
  rwmsg_broker_main(sid, instid, bro_instid, rws, tinfo, (rwcal_module_ptr_t) rwcal, usemainq, NULL, (rwmsg_broker_t **) bro_out);
}
