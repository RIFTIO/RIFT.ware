
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rw_resource_track.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 03/10/14
 * @header file for resource-tracking
 */

#ifndef __RW_RESOURCE_TRACK_H__
#define __RW_RESOURCE_TRACK_H__

#ifndef __GI_SCANNER__

#ifdef _CF_
#include <CoreFoundation/CFRuntime.h>
#endif

#include "stdio.h"
#include "rwlib.h"
#include "rw_object.h"
#include "talloc.h"
#include "pthread.h"

extern _RW_THREAD_ RW_RESOURCE_TRACK_HANDLE g_rwresource_track_handle;
extern pthread_mutex_t g_rwresource_track_mutex;
extern int g_malloc_intercepted;
extern int g_callstack_depth;
extern int g_heap_track_nth;
extern int g_heap_track_bigger_than;
extern int g_heap_decode_using;

#if 1
#define RW_RESOURCE_TRACK_LOCK()    pthread_mutex_lock(&g_rwresource_track_mutex)
#define RW_RESOURCE_TRACK_UNLOCK()  pthread_mutex_unlock(&g_rwresource_track_mutex)
#else
#define RW_RESOURCE_TRACK_LOCK()
#define RW_RESOURCE_TRACK_UNLOCK()
#endif

__BEGIN_DECLS

typedef struct rw_ta_res_track_s {
  void *addr;
  union {
    const char *location;
    void *callers[RW_RESOURCE_TRACK_MAX_CALLERS];
  };
} rw_ta_res_track_t;

#define RW_TA_DATA_SIZE sizeof(rw_ta_res_track_t)

#ifdef _CF_
#define RW_CF_OFFSET sizeof(CFAllocatorRef)
#else // _CF_
#define RW_CF_OFFSET 0
#endif // _CF_


typedef struct rw_mem_res_track_s rw_mem_res_track_t;
typedef rw_ta_res_track_t * _RW_TA_RESOURCE_TRACK_HANDLE_;
typedef rw_mem_res_track_t * _RW_MEM_RESOURCE_TRACK_HANDLE_;
typedef void* RW_RESOURCE_TRACK_HANDLE;


static inline int _rw_ta_destructor(void *handle)
{
  _RW_TA_RESOURCE_TRACK_HANDLE_ rh;
  char *memblock;

  rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)handle;
  if (rh->addr) {
    memblock = ((char *) rh->addr - RW_MAGIC_PAD);
    if (((rw_mem_res_track_t*)(memblock + sizeof(rw_magic_pad_t)))->obj_type == RW_CF_OBJECT) {
#ifdef _CF_
      CFRelease(memblock + RW_MAGIC_PAD + RW_CF_OFFSET);
#endif // _CF_
    } else {
      RW_FREE(memblock);
    }
    rh->addr = NULL;
  }
  else {
    //RW_CRASH();
  }
  return 0;
}

static inline _RW_TA_RESOURCE_TRACK_HANDLE_
_rw_ta_resource_track_create_context(const char *descr)
{
  _RW_TA_RESOURCE_TRACK_HANDLE_ rh;

  RW_RESOURCE_TRACK_LOCK();

  rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)talloc_named_const(NULL, RW_TA_DATA_SIZE, descr);
  memset(rh, '\0', RW_TA_DATA_SIZE);
  talloc_set_destructor((void*)rh, _rw_ta_destructor);
  //rh->addr = NULL;

  RW_RESOURCE_TRACK_UNLOCK();

  return rh;
}

static inline void
_rw_ta_resource_track_free(void *rh)
{
  RW_RESOURCE_TRACK_LOCK();

  talloc_free(rh);

  RW_RESOURCE_TRACK_UNLOCK();
}

static inline void
_rw_ta_resource_track_free_addr(void *addr, rw_magic_pad_t hash)
{
  _RW_MEM_RESOURCE_TRACK_HANDLE_ rc_mem;
  char *memblock;
  void *paddr;

  memblock = ((char *)addr - RW_MAGIC_PAD);

  if (hash) RW_ASSERT(*((rw_magic_pad_t *) memblock) == (hash ^ (rw_magic_pad_t) memblock));

  rc_mem = (_RW_MEM_RESOURCE_TRACK_HANDLE_)(memblock + sizeof(rw_magic_pad_t));

  RW_ASSERT(rc_mem->obj_type == RW_CF_OBJECT ||
            rc_mem->obj_type == RW_RAW_OBJECT ||
            rc_mem->obj_type == RW_MALLOC_OBJECT);

  if ((paddr = rc_mem->addr) != NULL) {
    //_RW_TA_RESOURCE_TRACK_HANDLE_ rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)rc_mem->addr;
    //fprintf(stderr, "F %s %p %p %s\n", rh->location, rh->addr, rc_mem->addr, talloc_get_name(rc_mem->addr));

    RW_ASSERT(talloc_total_blocks(rc_mem->addr) == 1);
    RW_ASSERT(talloc_reference_count(rc_mem->addr) == 0);
    rc_mem->addr = NULL;
    _rw_ta_resource_track_free(paddr);
  }
  else {
    rw_free_magic(addr, hash);
  }
}

static inline _RW_TA_RESOURCE_TRACK_HANDLE_
_rw_ta_resource_track_attach_child(void *prh,
                                   void *addr,
                                   unsigned int size,
                                   rw_object_type_t obj_type,
                                   const char *descr,
                                   rw_magic_pad_t hash,
                                   const char *loc)
{
  _RW_TA_RESOURCE_TRACK_HANDLE_ rh;
  _RW_MEM_RESOURCE_TRACK_HANDLE_ rc_mem;
  char *memblock;

  RW_RESOURCE_TRACK_LOCK();

  rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)talloc_named_const(prh, RW_TA_DATA_SIZE, descr);
  RW_ASSERT(rh);
  memset(rh, '\0', RW_TA_DATA_SIZE);
  talloc_set_destructor((void*)rh, _rw_ta_destructor);

  RW_RESOURCE_TRACK_UNLOCK();

  if (obj_type == RW_CF_OBJECT) {
    rh->addr = ((char*)addr - RW_CF_OFFSET); // Thre is a CF overhead
  }
  else if (obj_type == RW_RAW_OBJECT ||
           obj_type == RW_MALLOC_OBJECT) {
    rh->addr = addr;
  }
  else {
    RW_CRASH();
  }
  memblock = ((char *)rh->addr - RW_MAGIC_PAD);

  if (hash) RW_ASSERT(*((rw_magic_pad_t *) memblock) == (hash ^ (rw_magic_pad_t) memblock));

  rc_mem = (_RW_MEM_RESOURCE_TRACK_HANDLE_)(memblock + sizeof(rw_magic_pad_t));

  rc_mem->parent = prh;
  rc_mem->size = size;
  if (obj_type == RW_MALLOC_OBJECT) {
    int i;
    for (i=0; i<g_callstack_depth; i++) {
      rh->callers[i] = ((void**)loc)[i];
    }
  } else {
    rh->location = loc;
  }
  rc_mem->addr = rh;
  rc_mem->obj_type = obj_type;

  //fprintf(stderr, "%s %p %p %s\n", rh->location, rh->addr, rc_mem->addr, talloc_get_name(rc_mem->addr));

  return rh;
}

static inline _RW_TA_RESOURCE_TRACK_HANDLE_
_rw_ta_resource_track_reparent(void *addr,
                              void *nprh,
                              const char *loc)
{
  _RW_TA_RESOURCE_TRACK_HANDLE_ prh, rh;
  _RW_MEM_RESOURCE_TRACK_HANDLE_ rc_mem;
  char *memblock;

  UNUSED(loc);

  memblock = ((char *)addr - RW_MAGIC_PAD);

  rc_mem = (_RW_MEM_RESOURCE_TRACK_HANDLE_)(memblock + sizeof(rw_magic_pad_t));

  prh = (_RW_TA_RESOURCE_TRACK_HANDLE_)rc_mem->parent;
  rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)rc_mem->addr;

  RW_RESOURCE_TRACK_LOCK();

  talloc_reparent(prh, nprh, rh);

  rc_mem->parent = nprh;

  RW_RESOURCE_TRACK_UNLOCK();

  return rh;
}

static inline void
_rw_ta_resource_track_remove(void *addr,
                             rw_object_type_t obj_type,
                             rw_magic_pad_t hash,
                             bool_t and_free,
                             const char *loc)
{
  _RW_TA_RESOURCE_TRACK_HANDLE_ rh = NULL;
  _RW_MEM_RESOURCE_TRACK_HANDLE_ rc_mem;
  char *memblock;

  UNUSED(loc);

  if (obj_type == RW_CF_OBJECT) {
    memblock = ((char *)addr - RW_CF_OFFSET - RW_MAGIC_PAD); // Thre is a CF overhead
  }
  else if (obj_type == RW_RAW_OBJECT ||
           obj_type == RW_MALLOC_OBJECT){
    memblock = ((char *)addr - RW_MAGIC_PAD);
  }
  else {
    RW_CRASH();
  }

  if (hash) RW_ASSERT(*((rw_magic_pad_t *) memblock) == (hash ^ (rw_magic_pad_t) memblock));

  rc_mem = (_RW_MEM_RESOURCE_TRACK_HANDLE_)(memblock + sizeof(rw_magic_pad_t));

  rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)(rc_mem->addr);
  if (rh) {
    rh->addr = NULL;
    rc_mem->addr = NULL;

    RW_RESOURCE_TRACK_LOCK();

    if (and_free)
      talloc_free(rh);
    else
      talloc_set_name_const(rh, "FREED");

    RW_RESOURCE_TRACK_UNLOCK();
  }
}

extern char* rw_btrace_get_proc_name(void *addr);

static inline void
RW_talloc_report_depth_FILE_helper(const void *ptr, int depth, int max_depth, int is_ref, void *_f)
{
  _RW_TA_RESOURCE_TRACK_HANDLE_ rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)ptr;
  const char *name;
  FILE *f = (FILE *)_f;
  _RW_MEM_RESOURCE_TRACK_HANDLE_ rc_mem;
  char *memblock;
  void *paddr;

  UNUSED(is_ref);

  name = talloc_get_name(ptr);

  if (depth == 0) {
    fprintf(f,"%stalloc report on '%s' (total %6lu bytes in %3lu blocks)\n",
            (max_depth < 0 ? "full " :""), name,
            (unsigned long)talloc_total_size(ptr),
            (unsigned long)talloc_total_blocks(ptr));
    return;
  }

  if (rh->addr && strcmp(name, "FREED")) {
#if 0
    fprintf(f, "%*s%-30s %3lu blocks (ref %d) %p\n",
            depth*4, "",
            name,
            //(unsigned long)talloc_total_size(ptr),
            (unsigned long)talloc_total_blocks(ptr),
            (int)talloc_reference_count(ptr), rh->addr);
#else
  memblock = ((char *)rh->addr - RW_MAGIC_PAD);
  rc_mem = (_RW_MEM_RESOURCE_TRACK_HANDLE_)(memblock + sizeof(rw_magic_pad_t));
  if (rc_mem->obj_type == RW_CF_OBJECT ||
      rc_mem->obj_type == RW_RAW_OBJECT ||
      rc_mem->obj_type == RW_MALLOC_OBJECT) {
    paddr = rc_mem->obj_type == RW_CF_OBJECT ? ((char *)(rh->addr) + RW_CF_OFFSET) : rh->addr;

    if (rc_mem->obj_type == RW_MALLOC_OBJECT) {
      fprintf(f, "%*s%-30s(%u) %p\n", 
              depth*4, "",
              name, rc_mem->size, paddr);
      int i;
      for (i=0; i<RW_RESOURCE_TRACK_MAX_CALLERS && rh->callers[i]; i++) {
        fprintf(f, "%*s    %s\n", depth*4, "", rw_btrace_get_proc_name(rh->callers[i]));
      }
    } else {
      fprintf(f, "%*s%-30s(%u) %p %s\n",
              depth*4, "",
              name, rc_mem->size, paddr, rh->location);
    }
  }
#endif
  }
}

static inline void
_rw_ta_resource_track_dump(void *rh, void *_f)
{
  RW_RESOURCE_TRACK_LOCK();

  talloc_report_depth_cb(rh, 0, -1, RW_talloc_report_depth_FILE_helper, _f);

  RW_RESOURCE_TRACK_UNLOCK();
}

typedef struct {
  void *sink;
  unsigned int sink_len;
  unsigned int sink_used;
  rw_object_type_t obj_type;
  union {
    const char *loc;
    void *callers[RW_RESOURCE_TRACK_MAX_CALLERS];
  };
  const char *name;
  unsigned int size;
  unsigned int count;
} _res_track_report_sink_t_;

typedef _res_track_report_sink_t_ * RW_TA_RESOURCE_TRACK_DUMP_SINK;

static inline void
RW_talloc_report_depth_STRING_helper(const void *ptr, int depth, int max_depth, int is_ref, void *_s)
{
  _RW_TA_RESOURCE_TRACK_HANDLE_ rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)ptr;
  const char *name;
  _res_track_report_sink_t_ *s = (_res_track_report_sink_t_ *)_s;
  _RW_MEM_RESOURCE_TRACK_HANDLE_ rc_mem;
  char *memblock;
  void *paddr;
  unsigned int used;
  char *sink = (char*)s->sink;

  UNUSED(is_ref);

  name = talloc_get_name(ptr);

  if (depth == 0) {
    used = snprintf(NULL, 0, "%stalloc report on '%s' (total %6lu bytes in %3lu blocks)\n",
            (max_depth < 0 ? "full " :""), name,
            (unsigned long)talloc_total_size(ptr),
            (unsigned long)talloc_total_blocks(ptr));
    if (s->sink_used+used < s->sink_len) {
      snprintf(sink+s->sink_used, s->sink_len-s->sink_used, "%stalloc report on '%s' (total %6lu bytes in %3lu blocks)\n",
            (max_depth < 0 ? "full " :""), name,
            (unsigned long)talloc_total_size(ptr),
            (unsigned long)talloc_total_blocks(ptr));
      s->sink_used += used;
    }
    return;
  }

  if (rh->addr && strcmp(name, "FREED")) {
    memblock = ((char *)rh->addr - RW_MAGIC_PAD);
    rc_mem = (_RW_MEM_RESOURCE_TRACK_HANDLE_)(memblock + sizeof(rw_magic_pad_t));
    paddr = rc_mem->obj_type == RW_CF_OBJECT ? ((char *)(rh->addr) + RW_CF_OFFSET) : rh->addr;

    used = snprintf(NULL, 0, "%*s%-30s(%u) %p %s\n",
            depth*4, "",
            name, rc_mem->size, paddr, rh->location);
    if (s->sink_used+used < s->sink_len) {
      snprintf(sink+s->sink_used, s->sink_len-s->sink_used, "%*s%-30s(%u) %p %s\n",
            depth*4, "",
            name, rc_mem->size, paddr, rh->location);
      s->sink_used += used;
    }
  }
}

static inline void
_rw_ta_resource_track_dump_string(void *rh, char *str, unsigned int str_len)
{
  _res_track_report_sink_t_ sink;
  RW_RESOURCE_TRACK_LOCK();

  sink.sink = str;
  sink.sink_len = str_len;
  sink.sink_used = 0;

  talloc_report_depth_cb(rh, 0, -1, RW_talloc_report_depth_STRING_helper, (void*)&sink);

  RW_RESOURCE_TRACK_UNLOCK();
}

static inline void
RW_talloc_report_depth_STRUCT_helper(const void *ptr, int depth, int max_depth, int is_ref, void *_s)
{
  _RW_TA_RESOURCE_TRACK_HANDLE_ rh = (_RW_TA_RESOURCE_TRACK_HANDLE_)ptr;
  const char *name;
  _res_track_report_sink_t_ *s = (_res_track_report_sink_t_ *)_s;
  _RW_MEM_RESOURCE_TRACK_HANDLE_ rc_mem;
  char *memblock;
  void *paddr;

  UNUSED(is_ref);
  UNUSED(max_depth);

  if (s->sink_len == s->sink_used) return;

  name = talloc_get_name(ptr);

  if (depth == 0) {
    s[s->sink_used].sink = NULL;
    s[s->sink_used].name = name;
    s[s->sink_used].size = 0;
    s[s->sink_used].loc = NULL;
    s[s->sink_used].count = 0;
    s->sink_used ++;
    return;
  }

  if (rh->addr && strcmp(name, "FREED")) {
    memblock = ((char *)rh->addr - RW_MAGIC_PAD);
    rc_mem = (_RW_MEM_RESOURCE_TRACK_HANDLE_)(memblock + sizeof(rw_magic_pad_t));
#if 0
    unsigned int i;
    for (i=0; i<s->sink_used; i++) {
      if (!strcmp(s[i].name, name) && (s[i].loc == rh->location)) {
        s[i].count++;
        return;
      }
    }
#endif

    if (rc_mem->obj_type == RW_CF_OBJECT ||
        rc_mem->obj_type == RW_RAW_OBJECT ||
        rc_mem->obj_type == RW_MALLOC_OBJECT) {
        paddr = rc_mem->obj_type == RW_CF_OBJECT ? ((char *)(rh->addr) + RW_CF_OFFSET) : rh->addr;
        s[s->sink_used].sink = (void*)paddr;
        s[s->sink_used].name = name;
        s[s->sink_used].size = rc_mem->size;
        s[s->sink_used].obj_type = rc_mem->obj_type;
        if (rc_mem->obj_type == RW_MALLOC_OBJECT) {
          int i;
          for (i=0; i<RW_RESOURCE_TRACK_MAX_CALLERS; i++) {
            s[s->sink_used].callers[i] = rh->callers[i];
          }
        } else {
          RW_ASSERT(rc_mem->obj_type == RW_RAW_OBJECT ||
                    rc_mem->obj_type == RW_CF_OBJECT);
          s[s->sink_used].loc = rh->location;
        }
        s[s->sink_used].count = 0;
        s->sink_used ++;
    }
  }
}

static inline void
_rw_ta_resource_track_dump_struct(void *rh, RW_TA_RESOURCE_TRACK_DUMP_SINK s, unsigned int s_len)
{
  RW_TA_RESOURCE_TRACK_DUMP_SINK sink = s;
  RW_RESOURCE_TRACK_LOCK();

  sink->sink_len = s_len;

  talloc_report_depth_cb(rh, 0, -1, RW_talloc_report_depth_STRUCT_helper, (void*)sink);

  RW_RESOURCE_TRACK_UNLOCK();
}

static inline unsigned int
_rw_ta_resource_track_count(void *rh)
{
  int count;

  RW_RESOURCE_TRACK_LOCK();

  count = talloc_reference_count(rh);;

  RW_RESOURCE_TRACK_UNLOCK();

  return count;
}


//////////////////////////// PUBLIC MACROS ####################################
#define RW_RESOURCE_TRACK_CREATE_CONTEXT(descr) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_create_context(descr)

#define RW_RESOURCE_TRACK_FREE(rh) _rw_ta_resource_track_free(rh)

#define RW_RESOURCE_TRACK_REPARENT(addr, nprh) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_reparent(addr, nprh, G_STRLOC)

#define RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, addr, type) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, 0, RW_RAW_OBJECT, RW_QUOTE(type),\
                                                                 RW_HASH_TYPE(type), G_STRLOC)

#define RW_RESOURCE_TRACK_TYPE_ATTACH_CHILD_W_LOC(rh, addr, size, type, loc) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, size, RW_RAW_OBJECT, RW_QUOTE(type),\
                                                                 RW_HASH_TYPE(type), loc)

#define RW_RESOURCE_TRACK_ATTACH_CHILD(rh, addr, size, descr) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, size, RW_RAW_OBJECT, descr, 0, G_STRLOC)

#define RW_RESOURCE_TRACK_ATTACH_CHILD_W_LOC(rh, addr, size, descr, loc) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, size, RW_RAW_OBJECT, descr, 0, loc)

#define RW_MALLOC_RESOURCE_TRACK_CALLERS 5
#define RW_MALLOC_RESOURCE_TRACK_ATTACH_CHILD_W_LOC(rh, addr, size, descr, loc) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, size, RW_MALLOC_OBJECT, descr, 0, loc)


#define RW_RESOURCE_TRACK_TYPE_REMOVE_TRACKING(addr, type) \
    _rw_ta_resource_track_remove(addr, RW_RAW_OBJECT, RW_HASH_TYPE(type), 0, G_STRLOC)

#define RW_RESOURCE_TRACK_REMOVE_TRACKING(addr) \
    _rw_ta_resource_track_remove(addr, RW_RAW_OBJECT, 0, 0, G_STRLOC)

#define RW_RESOURCE_TRACK_REMOVE_TRACKING_AND_FREE(addr) \
    _rw_ta_resource_track_remove(addr, RW_RAW_OBJECT, 0, 1, G_STRLOC)

#define RW_RESOURCE_TRACK_DUMP(rh) _rw_ta_resource_track_dump(rh, stdout)

#define RW_RESOURCE_TRACK_DUMP_STRING(rh, str, str_len) _rw_ta_resource_track_dump_string(rh, str, str_len)

#define RW_RESOURCE_TRACK_DUMP_STRUCT(rh, s, s_len) _rw_ta_resource_track_dump_struct(rh, s, s_len)

#define RW_RESOURCE_TRACK_COUNT(rh) _rw_ta_resource_track_count(rh)

// RW_CF TYPE MACROS
#ifdef _CF_
#define RW_CF_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, addr, type) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, 0, RW_CF_OBJECT, RW_QUOTE(type),\
                                                                 RW_HASH_TYPE(type), G_STRLOC)

#define RW_CF_RESOURCE_TRACK_TYPE_ATTACH_CHILD_W_LOC(rh, addr, size, type, loc) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, size, RW_CF_OBJECT, RW_QUOTE(type),\
                                                                 RW_HASH_TYPE(type), loc)

#define RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, addr, descr) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, 0, RW_CF_OBJECT, descr, 0, G_STRLOC)

#define RW_CF_RESOURCE_TRACK_ATTACH_CHILD_W_LOC(rh, addr, size, descr, loc) \
    (RW_RESOURCE_TRACK_HANDLE)_rw_ta_resource_track_attach_child(rh, addr, size, RW_CF_OBJECT, descr, 0, loc)

#define RW_CF_RESOURCE_TRACK_REMOVE_TRACKING(addr) \
    _rw_ta_resource_track_remove(addr, RW_CF_OBJECT, 0, 0, G_STRLOC)
#else //_CF_
#define RW_CF_RESOURCE_TRACK_TYPE_ATTACH_CHILD(rh, addr, type) RW_STATIC_ASSERT(0)
#define RW_CF_RESOURCE_TRACK_ATTACH_CHILD(rh, addr, descr) RW_STATIC_ASSERT(0)
#define RW_CF_RESOURCE_TRACK_REMOVE_TRACKING(addr) RW_STATIC_ASSERT(0)
#endif // _CF_

// REDEFINING RW_FREE_TYPE
#undef RW_FREE_TYPE
#define RW_FREE_TYPE(addr, type) _rw_ta_resource_track_free_addr(addr, RW_HASH_TYPE(type))
#undef RW_MAGIC_FREE
#define RW_MAGIC_FREE(addr) _rw_ta_resource_track_free_addr(addr, 0);

__END_DECLS

#endif // __GI_SCANNER__
#endif /* __RW_RESOURCE_TRACK_H__ */
