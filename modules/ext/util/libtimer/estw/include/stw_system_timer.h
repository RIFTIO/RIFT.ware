/*------------------------------------------------------------------
 * stw_system_timer.h -- System Timer Wheel Definitions
 *
 * July 2009 - Bo Berry
 *
 * Copyright (c) 2005-2009 by Cisco Systems, Inc.
 * All rights reserved. 
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *------------------------------------------------------------------
 */

#ifndef __STW_SYSTEM_TIMER_H__
#define __STW_SYSTEM_TIMER_H__

#include "safe_types.h"
#include "stw_timer.h"


/*
 * Utility routine to initialize the links of timer elements.
 */
extern void
stw_system_timer_prepare(stw_tmr_t *tmr);


/*
 * Displays timer wheel stats and counters to stdout.
 */
extern void
stw_system_timer_stats(void);


/*
 * Returns the active status of a timer.
 */
extern boolean_t 
stw_system_timer_running(stw_tmr_t *tmr);


/*
 * Starts a new timer.  If the timer is currently 
 * running, it is stopped and restarted anew. 
 */
extern RC_STW_t
stw_system_timer_start(stw_tmr_t *tmr,
                     uint32_t delay,
                     uint32_t periodic_delay,
                stw_call_back user_cb,
                        void *parm);

/*
 * Stops a currently running timer
 */
extern RC_STW_t
stw_system_timer_stop(stw_tmr_t *);


/*
 * System Timer Wheel tick handler
 */
extern void
stw_system_timer_tick(void);


/*
 * destroy the R2CP timer wheel
 */
extern RC_STW_t
stw_system_timer_destroy(void);


/*
 * Creates and initializes the R2CP timer wheel
 */
extern RC_STW_t
stw_system_timer_create(uint32_t wheel_size,
                     uint32_t granularity,
                   const char *name);


#endif /* __STW_SYSTEM_TIMER_H__ */

