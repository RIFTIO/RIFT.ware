/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_cf_type_validate.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 02/12/14
 * @header file to define the macros for CF-izing a RW TYPE
 */

#ifndef __RW_CF_TYPE_VALIDATE_H__
#define __RW_CF_TYPE_VALIDATE_H__

#ifndef __GI_SCANNER__

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include "rwlib.h"
#include <rw_resource_track.h>

__BEGIN_DECLS

#define _RW_CF_TYPE_ID(type) __k__ ## type  ## __ID__
#define _RW_CF_CF_RUNTIME(type) __CF__ ## type ## __CF__

#define _RW_CF_TYPE_DESCRIPTION(type) "RIFT-CF Class - " #type


#define _RW_CF_TYPE_DEFINE(descr, type) \
  CFTypeID _RW_CF_TYPE_ID(type) = _kCFRuntimeNotATypeID; \
  static inline CFStringRef __## type ## __FormattingDesc(CFTypeRef cf, CFDictionaryRef formatOpts) { \
    UNUSED(cf); UNUSED(formatOpts);					\
    return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL,	\
                                    CFSTR( descr )); \
  } \
  static inline CFStringRef __## type ## __DebugDesc(CFTypeRef cf) { \
    UNUSED(cf); \
    return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, \
                                    CFSTR( descr )); \
  } \
  const CFRuntimeClass  _RW_CF_CF_RUNTIME(type) = { \
      _kCFRuntimeScannedObject, \
      _RW_CF_TYPE_DESCRIPTION(type), \
      NULL,  /* init */ \
      NULL,  /* copy */ \
      NULL,  /* dealloc */ \
      NULL,  /* equal */ \
      NULL,  /* hash */ \
      __## type ## __FormattingDesc, \
      __## type ## __DebugDesc \
    };

#define _RW_CF_TYPE_DECLARE(type) \
  extern CFTypeID _RW_CF_TYPE_ID(type); \
  extern const CFRuntimeClass  _RW_CF_CF_RUNTIME(type);

#define _RW_CF_TYPE_CLASS_DECLARE(type) \
  extern CFTypeID _RW_CF_TYPE_ID(type);

#define rw_malloc_free
static inline void  rw_cf_FreeInfo(const void  *info) {
#ifdef rw_malloc_free
  RW_FREE((void*)info);
#else
  free((void*)info);
#endif
}

static inline void * rw_cf_Alloc(CFIndex size, CFOptionFlags hint, void *info) {
  UNUSED(hint);
#ifdef rw_malloc_free
  return rw_malloc_magic(size, *((rw_magic_pad_t*)info), RW_CF_OBJECT);
#else
  UNUSED(info);
  return malloc(size);
#endif
}

static inline void * rw_cf_Realloc(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) {
  UNUSED(hint);
#ifdef rw_malloc_free
  return rw_realloc_magic(ptr, newsize, *((rw_magic_pad_t*)info));
#else
  UNUSED(info);
  return realloc(ptr, newsize);
#endif
}

static inline void rw_cf_Dealloc(void *ptr, void *info) {
#ifdef rw_malloc_free
  memset(ptr, '\0', (sizeof(CFRuntimeBase) + sizeof(CFAllocatorRef)));
  return rw_free_magic(ptr, *((rw_magic_pad_t*)info));
#else
  UNUSED(info);
  return free(ptr);
#endif
}

static inline CFStringRef rw_cf_CopyDescription(const void *info) {
  UNUSED(info);
  return CFSTR("My rw_cf_Allocator");
}

static inline CFIndex rw_cf_PreferredSize ( CFIndex size, CFOptionFlags hint, void *info) {
  UNUSED(size);
  UNUSED(hint);
  UNUSED(info);
  return size;
}

static inline CFAllocatorRef rw_cf_Allocator(const rw_magic_pad_t hash) {
  CFAllocatorRef allocator = NULL;
  if (!allocator) {
    CFAllocatorContext context =
     {
        0,                      //CFIndex version;
        NULL,                   //void *info;
        NULL,                   //CFAllocatorRetainCallBack
        rw_cf_FreeInfo,         //CFAllocatorReleaseCallBack
        rw_cf_CopyDescription,  //CFAllocatorCopyDescriptionCallBack
        rw_cf_Alloc,            //CFAllocatorAllocateCallBack
        rw_cf_Realloc,          //CFAllocatorReallocateCallBack
        rw_cf_Dealloc,          //CFAllocatorDeallocateCallBack
        rw_cf_PreferredSize,    //CFAllocatorPreferredSizeCallBack
    };
#ifdef rw_malloc_free
    context.info = RW_MALLOC(sizeof(rw_magic_pad_t));
#else
    context.info = malloc(sizeof(rw_magic_pad_t));
#endif
    *((rw_magic_pad_t*)context.info) = hash;
    allocator = CFAllocatorCreate(kCFAllocatorUseContext, &context);
  }
  return allocator;
}

static inline void *
rw_malloc0_cf_magic(gsize size, CFTypeID *typeID, rw_magic_pad_t hash, const char* type, const char* loc)
{
  char *memblock;
  memblock = (char *)_CFRuntimeCreateInstance(rw_cf_Allocator(hash), *typeID, size, NULL);
#ifdef _CF_
  if (g_rwresource_track_handle)
    RW_CF_RESOURCE_TRACK_ATTACH_CHILD_W_LOC(g_rwresource_track_handle, memblock, size, type, loc);
#else
  UNUSED(type);
  UNUSED(loc);
#endif
  return (void *) memblock;
}

extern void __CFGenericValidateType_(CFTypeRef cf, CFTypeID type, const char *func);
extern void __CFAssignTypeToMem(CFTypeRef cf, CFTypeID type);

#define _RW_CF_TYPE_VALIDATE(cf_object, type) \
    __CFGenericValidateType_(cf_object, _RW_CF_TYPE_ID(type), __PRETTY_FUNCTION__)

#define _RW_CF_TYPE_MALLOC0(size, type) \
    rw_malloc0_cf_magic(size, &_RW_CF_TYPE_ID(type), RW_HASH_TYPE(type), RW_QUOTE(type), G_STRLOC)
     
#define _RW_CF_TYPE_FREE(mem, type) \
    RW_CF_TYPE_VALIDATE(mem, type); \
    CFRelease(mem)

#define _RW_CF_TYPE_RETAIN(mem, type) \
    RW_CF_TYPE_VALIDATE(mem, type); \
    CFRetain(mem)
    
#define _RW_CF_TYPE_ASSIGN(mem, type) \
    __CFAssignTypeToMem(mem, _RW_CF_TYPE_ID(type))

#define _RW_CF_TYPE_REGISTER(type) \
{ \
  if (_kCFRuntimeNotATypeID == _RW_CF_TYPE_ID(type)) \
    _RW_CF_TYPE_ID(type) = _CFRuntimeRegisterClass(&_RW_CF_CF_RUNTIME(type)); \
}


///////////////////////////////////////////////////////////////////////////////

#define RW_CF_TYPE_DEFINE(descr, type)        _RW_CF_TYPE_DEFINE(descr, type)
#define RW_CF_TYPE_DECLARE(type)              _RW_CF_TYPE_DECLARE(type)
#define RW_CF_TYPE_EXTERN(type)               _RW_CF_TYPE_DECLARE(type)
#define RW_CF_TYPE_CLASS_EXTERN(type)         _RW_CF_TYPE_CLASS_DECLARE(type)
#define RW_CF_TYPE_REGISTER(type)             _RW_CF_TYPE_REGISTER(type)

#define RW_CF_TYPE_MALLOC0(size, type)        _RW_CF_TYPE_MALLOC0(size, type)
#define RW_CF_TYPE_FREE(mem, type)            _RW_CF_TYPE_FREE(mem, type)
#define RW_CF_TYPE_RETAIN(mem, type)          _RW_CF_TYPE_RETAIN(mem, type)
#define RW_CF_TYPE_RELEASE(mem, type)         _RW_CF_TYPE_FREE(mem, type)
#define RW_CF_TYPE_ASSIGN(mem, type)          _RW_CF_TYPE_ASSIGN(mem, type)

#define RW_CF_TYPE_VALIDATE(cf_object, type)  _RW_CF_TYPE_VALIDATE(cf_object, type)

#define RW_TYPE_DECL(name) typedef struct name##_s *name##_ptr_t, name##_t

#define RW_TYPE_DECL_ALL(s_type, c_type, ptr_type) typedef struct s_type c_type; typedef struct s_type* ptr_type;


__END_DECLS

#endif /* __GI_SCANNER__*/

#endif /* __RW_CF_TYPE_VALIDATE_H_ */
