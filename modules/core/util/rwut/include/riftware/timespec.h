/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file timespec.h
 * @author Austin Cormier (austin.cormier@riftio.com)
 * @date 05/06/2014
 * @brief Timespec helper functions
 */
#ifndef __TIMESPEC_H__
#define __TIMESPEC_H__

#include <cmath>
#include <string>
#include <stdio.h>

// Define NS_PER_S and NS_PER_MS if they haven't been defined elsewhere.
#ifndef NS_PER_S
#define NS_PER_S 1000000000L
#endif

#ifndef NS_PER_MS
#define NS_PER_MS 1000000L
#endif


/**
 * Take a start and end timespec and return a diff timespec representing the
 * time between the two
 *
 * @param[in] start - timspec representing the start time
 * @param[in] end - timespec representing the end time
 *
 * @returns timespec respresenting the difference between start and end time
 */
static inline timespec timespec_diff(timespec start, timespec end)
{
    timespec diff;

    if ((end.tv_nsec-start.tv_nsec) < 0) {
        diff.tv_sec = end.tv_sec - start.tv_sec-1;
        diff.tv_nsec = NS_PER_S + end.tv_nsec - start.tv_nsec;
    } else {
        diff.tv_sec = end.tv_sec - start.tv_sec;
        diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    return diff;
}

/**
 * Takes two timespec and return a timespec which is the summation of these two
 * inputs.
 *
 * @param[in] t1 - timespec input #1
 * @param[in] t2 - timespec input #2
 *
 * @returns timespec respresenting the summation of two inputs
 */
static inline timespec timespec_add(timespec t1, timespec t2)
{
    long sec = t2.tv_sec + t1.tv_sec;
    long nsec = t2.tv_nsec + t1.tv_nsec;
    if (nsec >= NS_PER_S) {
        nsec -= NS_PER_S;
        sec++;
    }

    return timespec{ .tv_sec = sec, .tv_nsec = nsec };
}

/**
 * Takes a timespec and a divisor as inputs and returns a timespec that is the
 * input timespec divided by the divisor.
 *
 * @param[in] time - timespec input
 * @param[in] divisor - divisor
 *
 * @returns timespec respresenting the input time divided by the divisor.
 */
static inline timespec timespec_divide(timespec time, unsigned int divisor)
{
    long ns_temp = time.tv_sec * NS_PER_S;
    ns_temp += time.tv_nsec;
    ns_temp /= divisor;

    timespec result;
    result.tv_sec = ns_temp / NS_PER_S;
    result.tv_nsec = ns_temp % NS_PER_S;

    return result;
}

/**
 * Takes a timespec which should represent a time difference and calculates
 * the number of nanoseconds between them.
 *
 * @param[in] time - timespec input
 *
 * @returns number of nanoseconds contained within the timespec
 */
static inline long int timespec_to_ns(timespec time)
{
    return (time.tv_sec) * NS_PER_S + time.tv_nsec;
}

#endif //__TIMESPEC_H__
