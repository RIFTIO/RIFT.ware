/*-----------------------------------------------------------------
 * stw_system_timer.c -- System Timer Wheel APIs
 *
 * February 2005, Bo Berry
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

#include "stw_system_timer.h"


static stw_t *stw_system_handle = 0;


/** 
 * NAME
 *    stw_system_timer_prepare
 *
 * SYNOPSIS 
 *    #include "stw_system_timer.h"
 *    void
 *    stw_system_timer_prepare(stw_tmr_t *tmr)
 * 
 * DESCRIPTION
 *    Utility routine to initialize the links of timer elements.
 * 
 * INPUT PARAMETERS 
 *    tmr - pointer to the timer
 * 
 * OUTPUT PARAMETERS 
 *    tmr - updated 
 * 
 * RETURN VALUE
 *    none
 * 
 */
void
stw_system_timer_prepare (stw_tmr_t *tmr)
{
    stw_timer_prepare(tmr);
    return;
}


/** 
 * NAME
 *    stw_system_timer_stats
 *
 * SYNOPSIS 
 *    #include "stw_system_timer.h"
 *    void
 *    stw_system_timer_stats(void)
 * 
 * DESCRIPTION
 *    Displays the stats for the system timer wheel.
 * 
 * INPUT PARAMETERS  
 *    none
 * 
 * OUTPUT PARAMETERS 
 *    none
 * 
 * RETURN VALUE
 *    none
 * 
 */
void
stw_system_timer_stats (void)
{
    stw_timer_stats(stw_system_handle);
    return;
}


/** 
 * NAME
 *    stw_system_timer_running
 *
 * SYNOPSIS 
 *    #include "stw_system_timer.h"
 *    boolean_t
 *    stw_system_timer_running(stw_tmr_t *tmr)
 *
 * DESCRIPTION
 *    Returns TRUE if the timer is active
 *
 * INPUT PARAMETERS  
 *    tmr - pointer to the timer
 * 
 * OUTPUT PARAMETERS 
 *    none
 *
 * RETURN VALUE
 *    TRUE==tmer is active
 *   False==timer is not active
 * 
 */
boolean_t 
stw_system_timer_running (stw_tmr_t *tmr)
{
    return (stw_timer_running(tmr));
}


/** 
 * NAME
 *    stw_system_timer_start
 *
 * SYNOPSIS 
 *    #include "stw_system_timer.h"
 *    RC_STW_t
 *    stw_system_timer_start(stw_tmr_t *tmr,
 *                            uint32_t  delay,
 *                            uint32_t  periodic_delay,
 *                       stw_call_back  user_cb,
 *                                void *parm)
 * 
 * DESCRIPTION
 *    Start (or restart) a system timer to expire
 *    in the future.  If the timer is currently active,
 *    it will be stopped first then restarted with
 *    time delay requested.
 * 
 * INPUT PARAMETERS
 *    tmr - pointer to the timer element
 *
 *    delay - specified delay in milliseconds
 *
 *    periodic_delay - periodic delay in milliseconds
 *
 *    user_cb - application function call-back to be
 *              invoked when the timer expires.  This 
 *              call-back must not block.
 *
 *    parm - persistent parameter passed to the application
 *           call-back upon expiration
 * 
 * OUTPUT PARAMETERS 
 *    tmr - updated with the user_cb and the parm, and 
 *          put on the tiemr wheel of fortune. 
 * 
 * RETURN VALUE
 *    RC_STW_OK
 *    error otherwise
 * 
 */
RC_STW_t
stw_system_timer_start (stw_tmr_t *tmr,
                         uint32_t  delay,
                         uint32_t  periodic_delay,
                    stw_call_back  user_cb,
                             void *parm)
{
    return (stw_timer_start(stw_system_handle,
                            tmr,
                            delay,
                            periodic_delay,
                            user_cb,
                            parm));
}


/** 
 * NAME
 *    stw_system_timer_stop
 *
 * SYNOPSIS 
 *    #include "stw_system_timer.h"
 *    RC_STW_t
 *    stw_system_timer_stop(stw_tmr_t *tmr)
 *
 * DESCRIPTION
 *    Stop the timer by removing it from the timer wheel.
 *    Note that it is safe to call this function with a
 *    timer that has already been stopped in order to
 *    avoid making all callers check for an active timer
 *    first.
 *
 * INPUT PARAMETERS
 *    tmr - pointer to the timer element
 *
 * OUTPUT PARAMETERS
 *    tmr - updated, taken off the wheel
 *
 * RETURN VALUE
 *    RC_STW_OK
 *    error otherwise
 * 
 */
RC_STW_t
stw_system_timer_stop (stw_tmr_t *tmr)
{
    return (stw_timer_stop(stw_system_handle, tmr));
}


/** 
 * NAME
 *    stw_system_timer_tick
 *
 * SYNOPSIS 
 *    #include "stw_system_timer.h"
 *    void
 *    stw_system_timer_tick(void)
 *
 * DESCRIPTION
 *    Expire timers on the current spoke and move
 *    the wheel cursor forward to the next spoke.
 *
 *    Application call-backs must be non-blocking
 *
 * INPUT PARAMETERS
 *    stw - pointer to the timer wheel
 *
 * OUTPUT PARAMETERS
 *    stw is updated, timers at the current tick are 
 *    expired. 
 *
 * RETURN VALUE
 *    none
 * 
 */
void
stw_system_timer_tick (void)
{
    /*
     * protect from the possibility of not having initialized the stw yet
     */
    if (stw_system_handle) {
        stw_timer_tick(stw_system_handle);
    }
    return;
}


/** 
 * NAME
 *    stw_system_timer_destroy
 *
 * SYNOPSIS 
 *    #include "stw_system_timer.h"
 *    RC_STW_t
 *    stw_system_timer_destroy(void)
 *
 * DESCRIPTION
 *    Destroys the timer wheel.  All timers are stopped 
 *    and resources released.
 *
 * INPUT PARAMETERS 
 *    none 
 *
 * OUTPUT PARAMETERS 
 *    none 
 *
 * RETURN VALUE
 *    RC_STW_OK
 *    error otherwise
 * 
 */
RC_STW_t
stw_system_timer_destroy (void)
{
    return (stw_timer_destroy(stw_system_handle)); 
}


/** 
 * NAME
 *    stw_system_timer_create
 *
 * SYNOPSIS 
 *    #include "stw_system_timer.h"
 *    RC_STW_t
 *    stw_system_timer_create(uint32_t  wheel_size,
 *                            uint32_t  granularity,
 *                           const char *p2name )
 * 
 * DESCRIPTION
 *     This function is used to create and initialize the system timer
 *     wheel.  Timers must not be started before this routine is called.
 * 
 * INPUT PARAMETERS  
 *    wheel_size - number of spokes in the wheel.  The number
 *                 of spoks should be engineered such that
 *                 wheel_size >= (longest duration / granularity )
 *                 Depending upon the number of concurrent timers, the
 *                 distribution of those timers, it may be beneficial to
 *                 further increase the wheel size.  Objective is to
 *                 minimize frequency of 'long' timers requiring wheel
 *                 revolutions.
 * 
 *    granularity - milliseconds between ticks
 * 
 *    *p2name - pointer to the name of identify wheel. 
 *              Limited to STW_NAME_LENGTH
 *
 * OUTPUT PARAMETERS
 *    none
 * 
 * RETURN VALUE
 *    RC_STW_OK
 *    error otherwise
 * 
 */
RC_STW_t
stw_system_timer_create (uint32_t  wheel_size,
                         uint32_t  granularity,
                       const char *p2name)
{
    return (stw_timer_create(wheel_size,
                             granularity,
                             p2name,
                            &stw_system_handle));
}


