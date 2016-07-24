
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwsched/core_foundation.h
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 2/24/2013
 * @brief Top level RW.Sched CoreFoundation API for tasklet aware CFRunLoop support
 */

#ifndef __RWSCHED_CORE_FOUNDATION_H__
#define __RWSCHED_CORE_FOUNDATION_H__

__BEGIN_DECLS

typedef struct {
  int dummy;
} rwtrack_object_t;

__END_DECLS

// Include the required cflite header files

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFSocket.h>

// Include the rwsched core foundation header files

#include <rwsched/cfrunloop.h>

#endif // __RWSCHED_CORE_FOUNDATION_H__
