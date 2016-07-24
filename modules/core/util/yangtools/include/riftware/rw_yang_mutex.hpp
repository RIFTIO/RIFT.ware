
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
