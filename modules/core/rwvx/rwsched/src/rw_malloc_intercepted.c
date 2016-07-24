
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __USE_GNU
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <ck.h>

#include "rwsched.h"
#include "rwsched_main.h"
#include "rwsched_internal.h"

#include "rwlib.h"

static void *(*callocp) (size_t, size_t);
static void *(*mallocp) (size_t);
static void *(*reallocp) (void *, size_t);
static void *(*memalignp)(size_t, size_t);
static void (*freep) (void *);

#define RETURN_ADDRESS(n) __builtin_return_address(n)
//#define MEM_ALLOC_PRINTF(fmt, _args...) fprintf(stderr, fmt, ##_args)
#define MEM_ALLOC_PRINTF(fmt, _args...)

typedef struct {
  unsigned int magic;
  unsigned int size;
  void *tasklet;
} rwmemory_padding_t;

#define MEM_ALLOC_PADDING (sizeof(rwmemory_padding_t)+RW_MAGIC_PAD)
#define RWMALLOC_MAGIC_TRACK 0xDEADFEED
#define RWMALLOC_MAGIC_NOTRACK 0xDEADFEEB

extern void *__libc_malloc(size_t);

int g_malloc_intercepted = 1;
int g_callstack_depth = 10;
int g_heap_track_nth = 1000;
int g_heap_track_bigger_than = 999;

/* PER THREAD GLOBAL VARIABLE - this is populated when scheduler dispatches events into  tasks */
__thread void *g_tasklet_info;
_RW_THREAD_ RW_RESOURCE_TRACK_HANDLE g_rwresource_track_handle;

static __thread int no_hook = 0;


static void __attribute__((constructor))
_alloc_init(void) {
  callocp = (void *(*) (size_t, size_t)) dlsym (RTLD_NEXT, "calloc");
  mallocp = (void *(*) (size_t)) dlsym (RTLD_NEXT, "malloc");
  reallocp = (void *(*) (void *, size_t)) dlsym (RTLD_NEXT, "realloc");
  memalignp = (void *(*)(size_t, size_t)) dlsym (RTLD_NEXT, "memalign");
  freep = (void (*) (void *)) dlsym (RTLD_NEXT, "free");
}

static inline void
_update_tasklet_memory_allocated(rwsched_tasklet_t *sched_tasklet, int size) {
  if (sched_tasklet) {
    // Validate input paraemters
    //RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
    RW_ASSERT(sched_tasklet);

    if (size<0) {
      ck_pr_sub_64(&sched_tasklet->counters.memory_allocated, -size);
      ck_pr_dec_64(&sched_tasklet->counters.memory_chunks);
    } else {
      ck_pr_add_64(&sched_tasklet->counters.memory_allocated, size);
      ck_pr_inc_64(&sched_tasklet->counters.memory_chunks);
    }

    MEM_ALLOC_PRINTF(" %lu ", sched_tasklet->counters.memory_allocated);
  }
}

static inline void
_fill_padding(rwmemory_padding_t *pad, size_t len, void *g_tasklet_info, int *track_this) {
  static int malloc_track_nth = 0;
  if (++malloc_track_nth>g_heap_track_nth
      || len>g_heap_track_bigger_than) {
    malloc_track_nth = 0;
    pad->magic = RWMALLOC_MAGIC_TRACK;
    pad->size = len;
    pad->tasklet = g_tasklet_info;
    memset((char*)(pad+1), '\0', RW_MAGIC_PAD);
    _update_tasklet_memory_allocated(g_tasklet_info, len);
    *track_this = 1;
  } else {
    pad->magic = RWMALLOC_MAGIC_NOTRACK;
    pad->size = len;
    pad->tasklet = g_tasklet_info;
    memset((char*)(pad+1), '\0', RW_MAGIC_PAD);
    *track_this = 0;
  }
}

static inline void
_add_tracking(void *addr, size_t size, const char* alloc_type, void *caller) {
  rwsched_tasklet_t *sched_tasklet = g_tasklet_info;
  RW_RESOURCE_TRACK_HANDLE rwresource_track_handle;
  
  rwresource_track_handle = sched_tasklet && sched_tasklet->rwresource_track_handle ?
                            sched_tasklet->rwresource_track_handle : g_rwresource_track_handle;

  if (addr && g_rwresource_track_handle) {
    RW_MALLOC_RESOURCE_TRACK_ATTACH_CHILD_W_LOC(rwresource_track_handle, addr, size, alloc_type, caller);
  }
}

static inline void
_remove_tracking(void *addr) {
    RW_RESOURCE_TRACK_REMOVE_TRACKING_AND_FREE(addr);
}

static inline void *
_do_malloc(size_t len, void *(*malloc_fn) (size_t), int *track_this) {
  void *ret = (*malloc_fn)(len+MEM_ALLOC_PADDING);
  if (ret) {
    _fill_padding((rwmemory_padding_t*)ret, len, g_tasklet_info, track_this);
    ret = (char*)ret+MEM_ALLOC_PADDING;
  }
  return ret;
}

static inline void *
_do_realloc(void *ptr, size_t len, void *(*realloc_fn) (void*, size_t), int *track_this) {
  void *ret;
  if (ptr) {
    if (*(int*)((char*)ptr-MEM_ALLOC_PADDING) == RWMALLOC_MAGIC_TRACK) {
      _remove_tracking(ptr);
      ptr = (char*)ptr-MEM_ALLOC_PADDING;
      rwmemory_padding_t *pad = (rwmemory_padding_t*)ptr;
      _update_tasklet_memory_allocated(pad->tasklet, -pad->size);
    } else if (*(int*)((char*)ptr-MEM_ALLOC_PADDING) == RWMALLOC_MAGIC_NOTRACK) {
      ptr = (char*)ptr-MEM_ALLOC_PADDING;
    }
  }
  ret = (*realloc_fn)(ptr, len+MEM_ALLOC_PADDING);
  if (ret) {
    _fill_padding((rwmemory_padding_t*)ret, len, g_tasklet_info, track_this);
    ret = (char*)ret+MEM_ALLOC_PADDING;
  }
  return ret;
}

static inline void
_do_free(void *ptr, void (*free_fn) (void*)) {
  if (ptr) {
    if (*(int*)((char*)ptr-MEM_ALLOC_PADDING) == RWMALLOC_MAGIC_TRACK) {
      _remove_tracking(ptr);
      ptr = (char*)ptr-MEM_ALLOC_PADDING;
      rwmemory_padding_t *pad = (rwmemory_padding_t*)ptr;
      _update_tasklet_memory_allocated(pad->tasklet, -pad->size);
    } else if (*(int*)((char*)ptr-MEM_ALLOC_PADDING) == RWMALLOC_MAGIC_NOTRACK) {
      ptr = (char*)ptr-MEM_ALLOC_PADDING;
    }
  }
  (*free_fn)(ptr);
}




/* PUBLIC FUNCTIONS */

void *calloc (size_t n, size_t len)
{
  void *ret;
  void *caller;
  int track_this = 0;

  if (no_hook) {
    if (callocp == NULL) {
      ret = _do_malloc(n*len, __libc_malloc, &track_this);
      memset((char*)ret, '\0', n*len);
     goto _return;
    }
    if (mallocp == NULL) {
      _alloc_init();
    }
    ret = _do_malloc(n*len, mallocp, &track_this);
    memset((char*)ret, '\0', n*len);
    goto _return;
  }
  no_hook = 1;
  if (mallocp == NULL) {
    _alloc_init();
  }
  ret = _do_malloc(n*len, mallocp, &track_this);
  memset((char*)ret, '\0', n*len);
  void *callers[RW_RESOURCE_TRACK_MAX_CALLERS+2];
  if (track_this) {
    int callstack_depth = g_callstack_depth;
    rw_btrace_backtrace(callers, callstack_depth+2);
    memset(&callers[callstack_depth+2], '\0',
           (RW_RESOURCE_TRACK_MAX_CALLERS-callstack_depth)*sizeof(callers[0]));
    caller = (void*)(&callers[2]);
    if (g_tasklet_info) MEM_ALLOC_PRINTF("%p-%p calloc(%zu, %zu", caller, g_tasklet_info, n, len);
    if (g_tasklet_info) MEM_ALLOC_PRINTF(") -> %p\n", ret);
    _add_tracking(ret, n*len, "calloc", (void*)(&callers[2]));
  }
  no_hook = 0;

_return:
  return ret;
  caller = caller;
  callers[0] = callers[0];
}

void *malloc (size_t len)
{
  void *ret;
  void *caller;
  int track_this = 0;

  if (mallocp == NULL) {
    _alloc_init();
  }
  if (no_hook) {
    ret = _do_malloc(len, mallocp, &track_this);
    goto _return;
  }
  no_hook = 1;
  ret = _do_malloc(len, mallocp, &track_this);
  void *callers[RW_RESOURCE_TRACK_MAX_CALLERS+2];
  if (track_this) {
    int callstack_depth = g_callstack_depth;
    rw_btrace_backtrace(callers, callstack_depth+2);
    memset(&callers[callstack_depth+2], '\0',
           (RW_RESOURCE_TRACK_MAX_CALLERS-callstack_depth)*sizeof(callers[0]));
    caller = (void*)(&callers[2]);
    if (g_tasklet_info) MEM_ALLOC_PRINTF("%p-%p malloc(%zu", caller, g_tasklet_info, len);
    if (g_tasklet_info) MEM_ALLOC_PRINTF(") -> %p\n", ret);
    _add_tracking(ret, len, "malloc", (void*)(&callers[2]));
  }
  no_hook = 0;

_return:
  return ret;
  caller = caller;
  callers[0] = callers[0];
}

void *realloc(void *ptr, size_t len)
{
  void *ret;
  void *caller;
  int track_this = 0;

  if (no_hook) {
    ret = _do_realloc(ptr, len, reallocp, &track_this);
    goto _return;
  }
  no_hook = 1;
  void *callers[RW_RESOURCE_TRACK_MAX_CALLERS+2];
  ret = _do_realloc(ptr, len, reallocp, &track_this);
  if (track_this) {
    int callstack_depth = g_callstack_depth;
    rw_btrace_backtrace(callers, callstack_depth+2);
    memset(&callers[callstack_depth+2], '\0',
           (RW_RESOURCE_TRACK_MAX_CALLERS-callstack_depth)*sizeof(callers[0]));
    caller = (void*)(&callers[2]);
    if (g_tasklet_info) MEM_ALLOC_PRINTF("%p-%p realloc(%p, %zu", caller, g_tasklet_info, ptr, len);
    if (g_tasklet_info) MEM_ALLOC_PRINTF(") -> %p\n", ret);
    _add_tracking(ret, len, "realloc", (void*)(&callers[2]));
  }
  no_hook = 0;

_return:
  return ret;
  caller = caller;
  callers[0] = callers[0];
}

void free (void *ptr)
{
  void *caller;
  if (no_hook) {
    _do_free(ptr, freep);
    goto _return;
  }
  no_hook = 1;
  caller = RETURN_ADDRESS(0);
  if (g_tasklet_info) MEM_ALLOC_PRINTF("%p-%p free(%p", caller, g_tasklet_info, ptr);
  _do_free(ptr, freep);
  if (g_tasklet_info) MEM_ALLOC_PRINTF(") -> \n");
  no_hook = 0;

_return:
  return;
  caller = caller;
}

void *memalign(size_t len, size_t size)
{
  void *ret;
  void *caller;
  if (no_hook) {
    ret = (*memalignp)(len, size);
    goto _return;
  }
  no_hook = 1;
  caller = RETURN_ADDRESS(0);
  if (g_tasklet_info) MEM_ALLOC_PRINTF("%p-%p memalign(%zu, %zu", caller, g_tasklet_info, len, size);
  ret = (*memalignp)(len, size);
  if (g_tasklet_info) MEM_ALLOC_PRINTF(") -> %p\n", ret);
  no_hook = 0;

_return:
  return ret;
  caller = caller;
}
