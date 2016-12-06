
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
