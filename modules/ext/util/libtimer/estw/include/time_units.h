/*----------------------------------------------------------------------
 * time_units.h
 *
 * January 2009, Bo Berry
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
 *---------------------------------------------------------------------
 */

#ifndef __TIME_UNITS_H__
#define __TIME_UNITS_H__

#include "safe_limits.h"
#include "safe_types.h"


#define NANOSECONDS_PER_SECONDS ( 1000 * 1000 * 1000 ) 
#define MICROSECONDS_PER_SECONDS ( 1000 * 1000 ) 
#define MILLISECONDS_PER_SECONDS ( 1000 ) 
#define DECISECONDS_PER_SECONDS ( 10 ) 


/*
 * Converts nanoseconds to microseconds. 
 * Can return 0.
 */ 
static inline uint32_t nano_to_microseconds (uint32_t nanoseconds)
{
    return (nanoseconds / 1000);
} 


/*
 * Converts nanoseconds to milliseconds. 
 * Can return 0.
 */ 
static inline uint32_t nano_to_milliseconds (uint32_t nanoseconds)
{
    return (nanoseconds / 1000 / 1000);
} 


/*
 * Converts nanoseconds to seconds. 
 * Can return 0.
 */ 
static inline uint32_t nano_to_seconds (uint32_t nanoseconds)
{
    return (nanoseconds / 1000 / 1000 / 1000);
} 



/*
 * Converts milliseconds to nanoseconds.  
 * RETURN VALUEs 0 if there is an overflow.
 */ 
static inline uint32_t milli_to_nanoseconds (uint32_t milliseconds)
{
    uint32_t nanoseconds;

    if (milliseconds > (UINT32_MAX / 1000 / 1000)) {
        nanoseconds = 0;
    } else { 
        nanoseconds = (milliseconds * 1000 * 1000);
    }

    return (nanoseconds);
}


/*
 * Converts milliseconds to microseconds.  
 * RETURN VALUEs 0 if there is an overflow.
 */ 
static inline uint32_t milli_to_microseconds (uint32_t milliseconds)
{
    uint32_t microseconds;

    if (milliseconds > (UINT32_MAX / 1000)) {
        microseconds = 0;
    } else { 
        microseconds = (milliseconds * 1000);
    }
    return (microseconds);
} 


/*
 * Converts milliseconds to seconds.  
 * Can return 0.
 */ 
static inline uint32_t milli_to_seconds (uint32_t milliseconds)
{
    return (milliseconds / 1000);
} 


/*
 * Converts seconds to nanoseconds.
 * RETURN VALUEs 0 if there is an overflow.
 */ 
static inline uint32_t secs_to_nanoseconds (uint32_t seconds)
{
    uint32_t nanoseconds;

    if (seconds > (UINT32_MAX / 1000 / 1000 / 1000)) {
        nanoseconds = 0; 
    } else {  
        nanoseconds = (seconds * 1000 * 1000 * 1000);
    } 
    return (nanoseconds); 
} 


/*
 * Converts seconds to microseconds.
 * RETURN VALUEs 0 if there is an overflow.
 */ 
static inline uint32_t secs_to_microseconds (uint32_t seconds)
{
    uint32_t microseconds;

    if (seconds > (UINT32_MAX / 1000 / 1000)) {
        microseconds = 0; 
    } else {  
        microseconds = (seconds * 1000 * 1000);
    } 
    return (microseconds); 
} 


/*
 * Converts seconds to milliseconds.
 * RETURN VALUEs 0 if there is an overflow.
 */ 
static inline uint32_t secs_to_milliseconds (uint32_t seconds)
{
    uint32_t milliseconds;

    if (seconds > (UINT32_MAX / 1000)) {
        milliseconds = 0; 
    } else {  
        milliseconds = (seconds * 1000);
    } 
    return (milliseconds); 
} 


#endif 

