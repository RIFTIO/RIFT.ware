/*!
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 * Creation Date: 9/11/15
 * 
 */

#ifndef RWMEMLOGDTS_H_
#define RWMEMLOGDTS_H_

#include <rwmemlog.h>

#include <rwtasklet.h>
#include <rwdts.h>

#include <sys/cdefs.h>

__BEGIN_DECLS

/*!
 * Register a memlog instance with DTS.
 */
rw_status_t rwmemlog_instance_dts_register(
  rwmemlog_instance_t* instance,
  rwtasklet_info_ptr_t tasklet_info,
  rwdts_api_t* apih );

/*!
 * Deregister a memlog instance with DTS.
 */
void rwmemlog_instance_dts_deregister(
  rwmemlog_instance_t* instance,
  bool dts_internal );


__END_DECLS

#endif /* ndef RWMEMLOGDTS_H_ */
