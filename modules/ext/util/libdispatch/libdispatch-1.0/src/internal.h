/*
 * Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

/*
 * IMPORTANT: This header file describes INTERNAL interfaces to libdispatch
 * which are subject to change in future releases of Mac OS X. Any applications
 * relying on these interfaces WILL break.
 */

#ifndef __DISPATCH_INTERNAL__
#define __DISPATCH_INTERNAL__

#include <config/config.h>

#define __DISPATCH_BUILDING_DISPATCH__
#define __DISPATCH_INDIRECT__

#ifdef __APPLE__
#include <Availability.h>
#include <TargetConditionals.h>
#endif

#if HAVE_DECL_TAILQ_FOREACH_SAFE
#include <sys/queue.h>
#else
#include "shims/queue.h"
#endif

#if !HAVE_STRLCPY
#include "shims/strlcpy.h"
#endif

// FIXME
#ifdef __BLOCKS__
#define WITH_DISPATCH_IO 1
#else 
#define WITH_DISPATCH_IO 0
#endif

#if USE_OBJC && ((!TARGET_IPHONE_SIMULATOR && defined(__i386__)) || \
		(!TARGET_OS_IPHONE && __MAC_OS_X_VERSION_MIN_REQUIRED < 1080))
// Disable Objective-C support on platforms with legacy objc runtime
#undef USE_OBJC
#define USE_OBJC 0
#endif

#if USE_OBJC
#define OS_OBJECT_HAVE_OBJC_SUPPORT 1
#if __OBJC__
#define OS_OBJECT_USE_OBJC 1
#else
#define OS_OBJECT_USE_OBJC 0
#endif // __OBJC__
#else
#define OS_OBJECT_HAVE_OBJC_SUPPORT 0
#endif // USE_OBJC

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif
#ifndef __has_include
#define __has_include(x) 0
#endif
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#include <dispatch/dispatch.h>
#include <dispatch/base.h>


#include <os/object.h>
#include <dispatch/object.h>
#include <dispatch/time.h>
#include <dispatch/queue.h>
#include <dispatch/source.h>
#include <dispatch/group.h>
#include <dispatch/semaphore.h>
#include <dispatch/once.h>
#include <dispatch/data.h>
#include <dispatch/io.h>

/* private.h must be included last to avoid picking up installed headers. */
#include "object_private.h"
#include "queue_private.h"
#include "source_private.h"
#include "data_private.h"
#include "benchmark.h"
#include "private.h"

/* More #includes at EOF (dependent on the contents of internal.h) ... */

// Abort on uncaught exceptions thrown from client callouts rdar://8577499
#if !defined(DISPATCH_USE_CLIENT_CALLOUT)
#define DISPATCH_USE_CLIENT_CALLOUT 1
#endif

/* The "_debug" library build */
#ifndef DISPATCH_DEBUG
#define DISPATCH_DEBUG 0
#endif

#ifndef DISPATCH_PROFILE
#define DISPATCH_PROFILE 0
#endif

#if (DISPATCH_DEBUG || DISPATCH_PROFILE) && !defined(DISPATCH_USE_DTRACE)
#define DISPATCH_USE_DTRACE 1
#endif

#if HAVE_LIBKERN_OSCROSSENDIAN_H
#include <libkern/OSCrossEndian.h>
#endif
#if HAVE_LIBKERN_OSATOMIC_H
#include <libkern/OSAtomic.h>
#endif
#if HAVE_MACH
#include <mach/boolean.h>
#include <mach/clock_types.h>
#include <mach/clock.h>
#include <mach/exception.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/mach_interface.h>
#include <mach/mach_time.h>
#include <mach/mach_traps.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/host_info.h>
#include <mach/notify.h>
#endif /* HAVE_MACH */
#if HAVE_MALLOC_MALLOC_H
#include <malloc/malloc.h>
#endif
#include <sys/event.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#ifdef __BLOCKS__
#include <Block_private.h>
#include <Block.h>
#endif /* __BLOCKS__ */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <search.h>
#if USE_POSIX_SEM
#include <semaphore.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#define DISPATCH_NOINLINE __attribute__((__noinline__))
#define DISPATCH_USED __attribute__((__used__))
#define DISPATCH_UNUSED __attribute__((__unused__))
#define DISPATCH_WEAK __attribute__((__weak__))
#if DISPATCH_DEBUG
#define DISPATCH_ALWAYS_INLINE_NDEBUG
#else
#define DISPATCH_ALWAYS_INLINE_NDEBUG __attribute__((__always_inline__))
#endif
#define DISPATCH_CONCAT(x,y) DISPATCH_CONCAT1(x,y)
#define DISPATCH_CONCAT1(x,y) x ## y

// workaround 6368156
#ifdef NSEC_PER_SEC
#undef NSEC_PER_SEC
#endif
#ifdef USEC_PER_SEC
#undef USEC_PER_SEC
#endif
#ifdef NSEC_PER_USEC
#undef NSEC_PER_USEC
#endif
#define NSEC_PER_SEC 1000000000ull
#define USEC_PER_SEC 1000000ull
#define NSEC_PER_USEC 1000ull

/* I wish we had __builtin_expect_range() */
#define fastpath(x) ((typeof(x))__builtin_expect((long)(x), ~0l))
#define slowpath(x) ((typeof(x))__builtin_expect((long)(x), 0l))

DISPATCH_NOINLINE
void _dispatch_bug(size_t line, long val);
DISPATCH_NOINLINE
void _dispatch_bug_client(const char* msg);
#if HAVE_MACH
DISPATCH_NOINLINE
void _dispatch_bug_mach_client(const char *msg, mach_msg_return_t kr);
#endif
DISPATCH_NOINLINE DISPATCH_NORETURN
void _dispatch_abort(size_t line, long val);
DISPATCH_NOINLINE __attribute__((__format__(__printf__,1,2)))
void _dispatch_log(const char *msg, ...);

/*
 * For reporting bugs within libdispatch when using the "_debug" version of the
 * library.
 */
#define dispatch_assert(e) do { \
		if (__builtin_constant_p(e)) { \
			char __compile_time_assert__[(bool)(e) ? 1 : -1] DISPATCH_UNUSED; \
		} else { \
			typeof(e) _e = fastpath(e); /* always eval 'e' */ \
			if (DISPATCH_DEBUG && !_e) { \
				_dispatch_abort(__LINE__, (long)_e); \
			} \
		} \
	} while (0)
/*
 * A lot of API return zero upon success and not-zero on fail. Let's capture
 * and log the non-zero value
 */
#define dispatch_assert_zero(e) do { \
		if (__builtin_constant_p(e)) { \
			char __compile_time_assert__[(bool)(e) ? -1 : 1] DISPATCH_UNUSED; \
		} else { \
			typeof(e) _e = slowpath(e); /* always eval 'e' */ \
			if (DISPATCH_DEBUG && _e) { \
				_dispatch_abort(__LINE__, (long)_e); \
			} \
		} \
	} while (0)

/*
 * For reporting bugs or impedance mismatches between libdispatch and external
 * subsystems. These do NOT abort(), and are always compiled into the product.
 *
 * In particular, we wrap all system-calls with assume() macros.
 */
#define dispatch_assume(e) ({ \
		typeof(e) _e = fastpath(e); /* always eval 'e' */ \
		if (!_e) { \
			if (__builtin_constant_p(e)) { \
				char __compile_time_assert__[(bool)(e) ? 1 : -1]; \
				(void)__compile_time_assert__; \
			} \
			_dispatch_bug(__LINE__, (long)_e); \
		} \
		_e; \
	})
/*
 * A lot of API return zero upon success and not-zero on fail. Let's capture
 * and log the non-zero value
 */
#define dispatch_assume_zero(e) ({ \
		typeof(e) _e = slowpath(e); /* always eval 'e' */ \
		if (_e) { \
			if (__builtin_constant_p(e)) { \
				char __compile_time_assert__[(bool)(e) ? -1 : 1]; \
				(void)__compile_time_assert__; \
			} \
			_dispatch_bug(__LINE__, (long)_e); \
		} \
		_e; \
	})

/*
 * For reporting bugs in clients when using the "_debug" version of the library.
 */
#define dispatch_debug_assert(e, msg, args...) do { \
		if (__builtin_constant_p(e)) { \
			char __compile_time_assert__[(bool)(e) ? 1 : -1] DISPATCH_UNUSED; \
		} else { \
			typeof(e) _e = fastpath(e); /* always eval 'e' */ \
			if (DISPATCH_DEBUG && !_e) { \
				_dispatch_log("%s() 0x%lx: " msg, __func__, (long)_e, ##args); \
				abort(); \
			} \
		} \
	} while (0)

/* Make sure the debug statments don't get too stale */
#define _dispatch_debug(x, args...) \
({ \
	if (DISPATCH_DEBUG) { \
		_dispatch_log("libdispatch: %u\t%p\t" x, __LINE__, \
				(void *)_dispatch_thread_self(), ##args); \
	} \
})

#if DISPATCH_DEBUG
#if HAVE_MACH
DISPATCH_NOINLINE DISPATCH_USED
void dispatch_debug_machport(mach_port_t name, const char* str);
#endif
DISPATCH_NOINLINE DISPATCH_USED
void dispatch_debug_kevents(struct kevent* kev, size_t count, const char* str);
#else
static inline void
dispatch_debug_kevents(struct kevent* kev DISPATCH_UNUSED,
		size_t count DISPATCH_UNUSED,
		const char* str DISPATCH_UNUSED) {}
#endif

#if DISPATCH_USE_CLIENT_CALLOUT

DISPATCH_NOTHROW void
_dispatch_client_callout(void *ctxt, dispatch_function_t f);
DISPATCH_NOTHROW void
_dispatch_client_callout2(void *ctxt, size_t i, void (*f)(void *, size_t));

#else

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_client_callout(void *ctxt, dispatch_function_t f)
{
	return f(ctxt);
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_client_callout2(void *ctxt, size_t i, void (*f)(void *, size_t))
{
	return f(ctxt, i);
}

#endif

#ifdef __BLOCKS__
DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_client_callout_block(dispatch_block_t b)
{
	struct Block_basic *bb = (struct Block_basic *)b;
	return _dispatch_client_callout(b, (dispatch_function_t)bb->Block_invoke);
}

dispatch_block_t _dispatch_Block_copy(dispatch_block_t block);
#define _dispatch_Block_copy(x) ((typeof(x))_dispatch_Block_copy(x))
void _dispatch_call_block_and_release(void *block);
#endif /* __BLOCKS__ */

void dummy_function(void);
long dummy_function_r0(void);
void _dispatch_vtable_init(void);

void _dispatch_source_drain_kevent(struct kevent *);

long _dispatch_update_kq(const struct kevent *);
void _dispatch_run_timers(void);
// Returns howsoon with updated time value, or NULL if no timers active.
struct timespec *_dispatch_get_next_timer_fire(struct timespec *howsoon);

uint64_t _dispatch_timeout(dispatch_time_t when);
#if USE_POSIX_SEM
struct timespec _dispatch_timeout_ts(dispatch_time_t when);
#endif

extern bool _dispatch_safe_fork;

extern struct _dispatch_hw_config_s {
	uint32_t cc_max_active;
	uint32_t cc_max_logical;
	uint32_t cc_max_physical;
} _dispatch_hw_config;

/* #includes dependent on internal.h */
#include "shims.h"

// Linux workarounds
// FIXME: There's no doubt a cleaner way to do this.
#if HAVE_PTHREAD_WORKQUEUES
#ifndef WORKQ_BG_PRIOQUEUE
#define DISPATCH_NO_BG_PRIORITY 1
#endif

#if !HAVE_PTHREAD_WORKQUEUE_SETDISPATCH_NP
#define DISPATCH_USE_LEGACY_WORKQUEUE_FALLBACK 1
#endif
#endif  // HAVE_PTHREAD_WORKQUEUES


// SnowLeopard and iOS Simulator fallbacks

#if HAVE_PTHREAD_WORKQUEUES
#ifndef WORKQ_BG_PRIOQUEUE
#define WORKQ_BG_PRIOQUEUE 3
#endif
#ifndef WORKQ_ADDTHREADS_OPTION_OVERCOMMIT
#define WORKQ_ADDTHREADS_OPTION_OVERCOMMIT 0x00000001
#endif
#if TARGET_IPHONE_SIMULATOR && __MAC_OS_X_VERSION_MIN_REQUIRED < 1070
#ifndef DISPATCH_NO_BG_PRIORITY
#define DISPATCH_NO_BG_PRIORITY 1
#endif
#endif
#if TARGET_IPHONE_SIMULATOR && __MAC_OS_X_VERSION_MIN_REQUIRED < 1080
#ifndef DISPATCH_USE_LEGACY_WORKQUEUE_FALLBACK
#define DISPATCH_USE_LEGACY_WORKQUEUE_FALLBACK 1
#endif
#endif
#if !TARGET_OS_IPHONE && __MAC_OS_X_VERSION_MIN_REQUIRED < 1080
#undef HAVE_PTHREAD_WORKQUEUE_SETDISPATCH_NP
#define HAVE_PTHREAD_WORKQUEUE_SETDISPATCH_NP 0
#endif
#endif // HAVE_PTHREAD_WORKQUEUES

#if HAVE_MACH
#if !defined(MACH_NOTIFY_SEND_POSSIBLE) || \
		(TARGET_IPHONE_SIMULATOR && __MAC_OS_X_VERSION_MIN_REQUIRED < 1070)
#undef MACH_NOTIFY_SEND_POSSIBLE
#define MACH_NOTIFY_SEND_POSSIBLE MACH_NOTIFY_DEAD_NAME
#endif
#endif // HAVE_MACH

#ifdef EVFILT_VM
#if TARGET_IPHONE_SIMULATOR && __MAC_OS_X_VERSION_MIN_REQUIRED < 1070
#undef DISPATCH_USE_MALLOC_VM_PRESSURE_SOURCE
#define DISPATCH_USE_MALLOC_VM_PRESSURE_SOURCE 0
#endif
#ifndef DISPATCH_USE_VM_PRESSURE
#define DISPATCH_USE_VM_PRESSURE 1
#endif
#ifndef DISPATCH_USE_MALLOC_VM_PRESSURE_SOURCE
#define DISPATCH_USE_MALLOC_VM_PRESSURE_SOURCE 1
#endif
#endif // EVFILT_VM

#if defined(F_SETNOSIGPIPE) && defined(F_GETNOSIGPIPE)
#if TARGET_IPHONE_SIMULATOR && __MAC_OS_X_VERSION_MIN_REQUIRED < 1070
#undef DISPATCH_USE_SETNOSIGPIPE
#define DISPATCH_USE_SETNOSIGPIPE 0
#endif
#ifndef DISPATCH_USE_SETNOSIGPIPE
#define DISPATCH_USE_SETNOSIGPIPE 1
#endif
#endif // F_SETNOSIGPIPE

#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif

#if __APPLE__
#define _dispatch_set_crash_log_message(x)
#else
#define _dispatch_set_crash_log_message(x) \
		syslog(LOG_USER|LOG_ERR, "[CRASH] %s", x)
#endif

#if HAVE_MACH
// MIG_REPLY_MISMATCH means either:
// 1) A signal handler is NOT using async-safe API. See the sigaction(2) man
//    page for more info.
// 2) A hand crafted call to mach_msg*() screwed up. Use MIG.
#define DISPATCH_VERIFY_MIG(x) do { \
		if ((x) == MIG_REPLY_MISMATCH) { \
			_dispatch_set_crash_log_message("MIG_REPLY_MISMATCH"); \
			_dispatch_hardware_crash(); \
		} \
	} while (0)
#endif

#define DISPATCH_CRASH(x) do { \
		_dispatch_set_crash_log_message("BUG IN LIBDISPATCH: " x); \
		_dispatch_hardware_crash(); \
	} while (0)

#define DISPATCH_CLIENT_CRASH(x) do { \
		_dispatch_set_crash_log_message("BUG IN CLIENT OF LIBDISPATCH: " x); \
		_dispatch_hardware_crash(); \
	} while (0)

#define _OS_OBJECT_CLIENT_CRASH(x) do { \
		_dispatch_set_crash_log_message("API MISUSE: " x); \
		_dispatch_hardware_crash(); \
	} while (0)

/* #includes dependent on internal.h */
#include "object_internal.h"
#include "semaphore_internal.h"
#include "queue_internal.h"
#include "source_internal.h"

#if WITH_DISPATCH_IO
#include "io_internal.h"
#include "data_internal.h"
#endif

#include "trace.h"

#endif /* __DISPATCH_INTERNAL__ */
