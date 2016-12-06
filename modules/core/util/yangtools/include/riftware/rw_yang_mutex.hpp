
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



/*!
 * @file rw_yang_mutex.hpp
 *
 * Global mutex support.
 *
 * Several components in the yang library require a mutex (or some
 * future advanced concurrency protection).  Previously, only
 * YangModelNcxImpl had an actual mutex, which was used globally for
 * all yang models.  The global mutex was factored out of
 * YangModelNcxImpl into this file in order to be shared by RW.XML.
 *
 * ATTN: Although making the mutex global and adding more users will
 * increase contention, ultimately, the mutex code will have to be
 * replaced with urcu or some other highly-concurrent library/data
 * structure.  In the mean time, we will eliminate the potential for
 * deadlock by having multiple mutexes spread out through the code.
 */

#ifndef RW_YANGTOOLS_YANG_MUTEX_HPP_
#define RW_YANGTOOLS_YANG_MUTEX_HPP_

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#include <mutex>

namespace rw_yang {

namespace GlobalMutex {

//! A global mutex type for the yang library
typedef std::recursive_mutex mutex_t;

//! A mutex guard for the global mutex
typedef std::lock_guard<mutex_t> guard_t;

//! Mutex for protecting synchronous yang library calls
extern mutex_t g_mutex;

} // namespace GlobalMutex

} // namespace rw_yang

#endif // RW_YANGTOOLS_YANG_MUTEX_HPP_
