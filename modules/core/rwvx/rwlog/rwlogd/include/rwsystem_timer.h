/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

/* Warapper for timer */
#ifndef __RWSYSTEM_TIMER__
#define __RWSYSTEM_TIMER__ 

#include "stw_system_timer.h"
typedef stw_tmr_t rw_tmr_t;
typedef stw_call_back rw_tmr_cb;

/*!
 * Call this routine to initialize the timer wheel
 * Should be called once during init.
 * @param hdl - system timer handler - pass by ref.
 * @param tasklet 
 * @param ms - number of milli seconds in which the timer wheel check for expiry
 * @param buckets - number of buckets 
 * @return timer identifier.
 */ 
rwsched_dispatch_source_t rwtimer_init(stw_t **hdl, rwtasklet_info_ptr_t rwtasklet, int ms, int buckets);

/*!
 * To deactivate the timer wheel and release the resources
 * @param - system timer handler
 * @param - tasklet 
 * @param - timer identifier returned by rwtimer_init()
 */
rw_status_t rwtimer_deinit(stw_t* hdl, rwtasklet_info_ptr_t rwtasklet, rwsched_dispatch_source_t id);

/*!
 * API to create a timer by applications
 *
 * CAUTION!! The callback should be a short and sweet non blocking function.
 *
 * @param hdl - system timer handle ( stored in rwlogd instance struct )
 * @param tmr - timer structure ( which is also used as arg to stop timer)
 * @param initial-delay - delay in milli seconds to fire timer
 * @param repeat interval - if zero timer wont repeat, or the repeat interval
 * @param cb - call back function when timer is expired
 * @param param - arguments to call back funciton
 */
rw_status_t rwtimer_start(stw_t * hdl, rw_tmr_t *tmr, uint32_t initial_delay,
            uint32_t repeat_interval, stw_call_back cb, void *param);

/*!
 * API to stop/cancel a running timer
 * @param hdl - system timer handle (stored in rwlogd instance struct) 
 * @param tmr - the timer returned by rwtimer_start
 */
rw_status_t rwtimer_stop(stw_t *hdl, rw_tmr_t *tmr);

#endif /* __RWSYSTEM_TIMER__ */
