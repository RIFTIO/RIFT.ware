/*
 * Copyright (c) 2008-2010 Apple Inc. All rights reserved.
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

#include "internal.h"

#pragma mark -
#pragma mark _os_object_t

unsigned long
_os_object_retain_count(_os_object_t obj)
{
	int xref_cnt = obj->os_obj_xref_cnt;
	if (slowpath(xref_cnt == _OS_OBJECT_GLOBAL_REFCNT)) {
		return ULONG_MAX; // global object
	}
	return xref_cnt + 1;
}

_os_object_t
_os_object_retain_internal(_os_object_t obj)
{
	int ref_cnt = obj->os_obj_ref_cnt;
	if (slowpath(ref_cnt == _OS_OBJECT_GLOBAL_REFCNT)) {
		return obj; // global object
	}
	ref_cnt = dispatch_atomic_inc2o(obj, os_obj_ref_cnt);
	if (slowpath(ref_cnt <= 0)) {
		DISPATCH_CRASH("Resurrection of an object");
	}
	return obj;
}

void
_os_object_release_internal(_os_object_t obj)
{
	int ref_cnt = obj->os_obj_ref_cnt;
	if (slowpath(ref_cnt == _OS_OBJECT_GLOBAL_REFCNT)) {
		return; // global object
	}
	ref_cnt = dispatch_atomic_dec2o(obj, os_obj_ref_cnt);
	if (fastpath(ref_cnt >= 0)) {
		return;
	}
	if (slowpath(ref_cnt < -1)) {
		DISPATCH_CRASH("Over-release of an object");
	}
#if DISPATCH_DEBUG
	if (slowpath(obj->os_obj_xref_cnt >= 0)) {
		DISPATCH_CRASH("Release while external references exist");
	}
#endif
	return _os_object_dispose(obj);
}

_os_object_t
_os_object_retain(_os_object_t obj)
{
	int xref_cnt = obj->os_obj_xref_cnt;
	if (slowpath(xref_cnt == _OS_OBJECT_GLOBAL_REFCNT)) {
		return obj; // global object
	}
	xref_cnt = dispatch_atomic_inc2o(obj, os_obj_xref_cnt);
	if (slowpath(xref_cnt <= 0)) {
		_OS_OBJECT_CLIENT_CRASH("Resurrection of an object");
	}
	return obj;
}

void
_os_object_release(_os_object_t obj)
{
	int xref_cnt = obj->os_obj_xref_cnt;
	if (slowpath(xref_cnt == _OS_OBJECT_GLOBAL_REFCNT)) {
		return; // global object
	}
	xref_cnt = dispatch_atomic_dec2o(obj, os_obj_xref_cnt);
	if (fastpath(xref_cnt >= 0)) {
		return;
	}
	if (slowpath(xref_cnt < -1)) {
		_OS_OBJECT_CLIENT_CRASH("Over-release of an object");
	}
	return _os_object_xref_dispose(obj);
}

bool
_os_object_retain_weak(_os_object_t obj)
{
	int xref_cnt = obj->os_obj_xref_cnt;
	if (slowpath(xref_cnt == _OS_OBJECT_GLOBAL_REFCNT)) {
		return true; // global object
	}
retry:
	if (slowpath(xref_cnt == -1)) {
		return false;
	}
	if (slowpath(xref_cnt < -1)) {
		goto overrelease;
	}
	if (slowpath(!dispatch_atomic_cmpxchg2o(obj, os_obj_xref_cnt, xref_cnt,
			xref_cnt + 1))) {
		xref_cnt = obj->os_obj_xref_cnt;
		goto retry;
	}
	return true;
overrelease:
	_OS_OBJECT_CLIENT_CRASH("Over-release of an object");
}

bool
_os_object_allows_weak_reference(_os_object_t obj)
{
	int xref_cnt = obj->os_obj_xref_cnt;
	if (slowpath(xref_cnt == -1)) {
		return false;
	}
	if (slowpath(xref_cnt < -1)) {
		_OS_OBJECT_CLIENT_CRASH("Over-release of an object");
	}
	return true;
}

#pragma mark -
#pragma mark dispatch_object_t

void *
_dispatch_alloc(const void *vtable, size_t size)
{
	return _os_object_alloc(vtable, size);
}

void
dispatch_retain(dispatch_object_t dou)
{
	(void)_os_object_retain(dou._os_obj);
}

void
_dispatch_retain(dispatch_object_t dou)
{
	(void)_os_object_retain_internal(dou._os_obj);
}

void
dispatch_release(dispatch_object_t dou)
{
	_os_object_release(dou._os_obj);
}

void
_dispatch_release(dispatch_object_t dou)
{
	_os_object_release_internal(dou._os_obj);
}

static void
_dispatch_dealloc(dispatch_object_t dou)
{
	dispatch_queue_t tq = dou._do->do_targetq;
	dispatch_function_t func = (dispatch_function_t)dou._do->do_finalizer;
	void *ctxt = dou._do->do_ctxt;

	_os_object_dealloc(dou._os_obj);

	if (func && ctxt) {
		dispatch_async_f(tq, ctxt, func);
	}
  if (tq) {
	  _dispatch_release(tq);
  }
}

void
_dispatch_xref_dispose(dispatch_object_t dou)
{
	if (slowpath(DISPATCH_OBJECT_SUSPENDED(dou._do))) {
		// Arguments for and against this assert are within 6705399
		DISPATCH_CLIENT_CRASH("Release of a suspended object");
	}
#if !USE_OBJC
	if (dx_type(dou._do) == DISPATCH_SOURCE_KEVENT_TYPE) {
		_dispatch_source_xref_dispose(dou._ds);
	}
	return _dispatch_release(dou._os_obj);
#endif
}

void
_dispatch_dispose(dispatch_object_t dou)
{
	if (slowpath(dou._do->do_next != DISPATCH_OBJECT_LISTLESS)) {
		DISPATCH_CRASH("Release while enqueued");
	}
	dx_dispose(dou._do);
	return _dispatch_dealloc(dou);
}

void *
dispatch_get_context(dispatch_object_t dou)
{
	return dou._do->do_ctxt;
}

void
dispatch_set_context(dispatch_object_t dou, void *context)
{
	if (dou._do->do_ref_cnt != DISPATCH_OBJECT_GLOBAL_REFCNT) {
		dou._do->do_ctxt = context;
	}
}

void
dispatch_set_finalizer_f(dispatch_object_t dou, dispatch_function_t finalizer)
{
	dou._do->do_finalizer = (void*)finalizer;
}

void
dispatch_suspend(dispatch_object_t dou)
{
	if (slowpath(dou._do->do_ref_cnt == DISPATCH_OBJECT_GLOBAL_REFCNT)) {
		return;
	}
	// rdar://8181908 explains why we need to do an internal retain at every
	// suspension.
	(void)dispatch_atomic_add2o(dou._do, do_suspend_cnt,
			DISPATCH_OBJECT_SUSPEND_INTERVAL);
	_dispatch_retain(dou._do);
}

DISPATCH_NOINLINE
static void
_dispatch_resume_slow(dispatch_object_t dou)
{
	_dispatch_wakeup(dou._do);
	// Balancing the retain() done in suspend() for rdar://8181908
	_dispatch_release(dou._do);
}

void
dispatch_resume(dispatch_object_t dou)
{
	// Global objects cannot be suspended or resumed. This also has the
	// side effect of saturating the suspend count of an object and
	// guarding against resuming due to overflow.
	if (slowpath(dou._do->do_ref_cnt == DISPATCH_OBJECT_GLOBAL_REFCNT)) {
		return;
	}
	// Check the previous value of the suspend count. If the previous
	// value was a single suspend interval, the object should be resumed.
	// If the previous value was less than the suspend interval, the object
	// has been over-resumed.
	unsigned int suspend_cnt = dispatch_atomic_sub2o(dou._do, do_suspend_cnt,
			DISPATCH_OBJECT_SUSPEND_INTERVAL) +
			DISPATCH_OBJECT_SUSPEND_INTERVAL;
	if (fastpath(suspend_cnt > DISPATCH_OBJECT_SUSPEND_INTERVAL)) {
		// Balancing the retain() done in suspend() for rdar://8181908
		return _dispatch_release(dou._do);
	}
	if (fastpath(suspend_cnt == DISPATCH_OBJECT_SUSPEND_INTERVAL)) {
		return _dispatch_resume_slow(dou);
	}
	DISPATCH_CLIENT_CRASH("Over-resume of an object");
}

size_t
_dispatch_object_debug_attr(dispatch_object_t dou, char* buf, size_t bufsiz)
{
	return snprintf(buf, bufsiz, "xrefcnt = 0x%x, refcnt = 0x%x, "
			"suspend_cnt = 0x%x, locked = %d, ", dou._do->do_xref_cnt + 1,
			dou._do->do_ref_cnt + 1,
			dou._do->do_suspend_cnt / DISPATCH_OBJECT_SUSPEND_INTERVAL,
			dou._do->do_suspend_cnt & 1);
}
