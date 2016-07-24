/*------------------------------------------------------------------
 * Single Timer Wheel Timer
 *
 * Copyright (c) 2002-2009 by Cisco Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *------------------------------------------------------------------
 */

A timer wheel is a very efficient timer facility that supports the 
typical timer features: start a timer, stop a timer expire a timer
and query a timer. The start timer API allows a single-shot timer 
and a repeating timer.  When a timer is started, the user specifies
the initial duration, repeating duration, the call-back function
to be invoked when the timer expires and a persistent parameter
that is passed to the user's call-back.  The source code for the 
timer wheel described can be down loaded from

           http://sourceforge.net/projects/estw/

The unit of time between each queue or spoke is the equal and 
constant. The queue scheme is a very efficient scheme to start 
and stop a timer.  For timers that have a duration longer than 
the rotation time, the rotation count will be decremented when 
its queue is selected and left for the next rotation.  This 
will continue until the rotation count is zero, indication that 
the timer has expired.  

The timer wheel is optimized to support embedded timer structures, 
where the timer data structure is integrated into the structure 
it is associated with.  When the timer is started, the structure 
is queued to the appropriate timer wheel queue. 

The timer wheel tick may be driven by a task loop or by a hardware 
interrupt.  It is possible to have multiple timer wheels in the 
system.  One could be a general purpose timer while another could
be dedicated to a specific application. 


Short List of APIs

/*
 * Starts a new timer.  If the timer is currently running,
 * it is stopped and restarted anew
 */
extern RC_STW_t
stw_timer_start(stw_t  *stw,
            stw_tmr_t  *tmr,
             uint32_t   delay,
             uint32_t   periodic_delay,
        stw_call_back   user_cb,
                void   *parm);


/*
 * stops a currently running timer
 */
extern RC_STW_t
stw_timer_stop(stw_t *stw, stw_tmr_t *tmr);


/*
 * Timer Wheel tick handler which drives time for the
 * specified wheel
 */
extern void
stw_timer_tick(stw_t *stw);


The timer wheel uses a 32-bit duration value.  The table 
below provides the longest durations given different timer tick
granularities. 

           Time per
             Tick         Duration
    ------------------------------------
             10ms          497.1 days
             20ms          994.2 days 
             50ms         2485.5 days 

    milliseconds per day [1000 * 60 * 60 * 24]


The design of the  timer wheel consists of a number of queues or 
spokes that are evenly spaced in time. With each tick, the timer 
wheel advances in a modulo fashion to the next queue or spoke.  
Every active timer on the spoke is set to expire on this tick. 
If the timer's rotational count is non-zero, the rotational count 
is decremented and left on the queue for the next turn.  If the 
rotational count is zero, the timer is expired.  Expiring a timer 
is simply invoking the user call-back function. 

          Wheel
         N Spokes
      +------------+
   0  |            |
      +------------+      +----------------+     
   1  |   first    |----->| rotation_count |------>
      |   last     |<-----| call-back      |<------
      |            |      | parameters     |
      +------------+      +----------------+  
      /            /
      +------------+
 N-1  |            |
      +------------+


Timer Wheel Demo

The timer wheel demo uses a single periodic linux alarm signal
to drive the timer wheel. 

    /* 
     * Install the interval timer_handler as the signal handler 
     * for SIGALRM. 
     */
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
    sigaction (SIGALRM, &sa, NULL);

 
This function is envoked as result of the sigalrm to drive the timer wheel.
   
   static void timer_handler (int signum)  
   {
       stw_system_timer_tick();  
   } 




The benefits of a timer wheel are many:
 
-The timer wheel can support a large number of timers. 

-The timer wheel supports single-shot and repeating timers. 

-The timer wheel is efficient, starting and stopping a timer is on
 the order of 1, O(1) 

-The timer wheel is scalable. The wheel size can be configured
 to an optimal number of timer queues. 

-The timer wheel does not require hashing or sorting overhead.

-The timer wheel memory requirements are minimual.  

-Multiple timer wheels can be integrated into a system, each 
 independent of the others.  



Definitions

Granularity - Granularity is the unit of timer between each spoke.
      Granularity is measured in time units, typically milliseconds.

Repeating Timer - A repeating timer is a timer that is started to
      automatically restart after each expiration.   

Rotation - Rotation is a complete rotation around the wheel where 
      all queues have been processed. 

Single-Shot Timer - a single-shot timer is a timer that is started
      to expire one and only one time. 

Spoke - A spoke refers to a queue of the timer wheel. 

Timer Wheel - A timer wheel is a data structure that consists of 
      multiple queues.  The unit of time between each queue is 
      constant. 



References

[1] http://www.ibm.com/developerworks/aix/library/au-lowertime/index.html

[2] jkkjjj



