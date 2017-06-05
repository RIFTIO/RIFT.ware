/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_cf_type_validate.hpp
 * @author Tom Seidenberg
 * @date 2014/06/24
 * @header file to define the macros for CF-izing a C++ class
 */

#ifndef __RW_CF_TYPE_VALIDATE_HPP__
#define __RW_CF_TYPE_VALIDATE_HPP__

#include <rw_cf_type_validate.h>

#ifdef __cplusplus

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#include <mutex>
#include <stddef.h>

#define _RW_CF_TYPE_CLASS_DECLARE_MEMBERS(c_t, ptr_t, cpp_t) \
  public: \
    /* Non-pointer C typedef */ \
    typedef ::c_t rw_mem_c_t; \
    \
    /* Pointered C typedef */ \
    typedef ::ptr_t rw_mem_ptr_t; \
    \
    /* CF-adaptation struct, for layout calculations only */ \
    struct rw_mem_cf_t : public rw_mem_c_t { \
      CFRuntimeBase cf_base_; \
      uintptr_t cpp_base_ __attribute__((aligned(__BIGGEST_ALIGNMENT__))); \
    }; \
    \
    /* unique_ptr<> that releases a reference. Hack, see below. */ \
    typedef UniquishPtrRwMemoryTracking<cpp_t>::suptr_t suptr_t; \
    \
    /* Retain a reference. */ \
    void rw_mem_retain() const; \
    \
    /* Release a reference. */ \
    void rw_mem_release() const; \
    \
    /* Get a duplicate reference. */ \
    suptr_t rw_mem_dupref(); \
    \
    /* Validate the CF tracking, using C pointer. */ \
    static void rw_mem_validate(const rw_mem_c_t* c_obj, const char* func); \
    \
    /* Validate the CF tracking, using member function. */ \
    void rw_mem_validate(const char* func) const; \
    \
    /* Convert void pointer to C++ pointer, if valid.  nullptr okay. */ \
    static cpp_t* rw_mem_cf_to_cpp_type(void* cf_obj); \
    \
    /* Convert CF reference to const C++ pointer, if valid.  nullptr okay. */ \
    static const cpp_t* rw_mem_cf_to_cpp_type(CFTypeRef cf_obj); \
    \
    /* Convert C pointer to C++ pointer.  Assumed valid.  nullptr okay. */ \
    static cpp_t* rw_mem_to_cpp_type(rw_mem_c_t* c_obj); \
    \
    /* Convert const C pointer to const C++ pointer.  Assumed valid.  nullptr okay. */ \
    static const cpp_t* rw_mem_to_cpp_type(const rw_mem_c_t* c_obj); \
    \
    /* Obtain C pointer from object. */ \
    rw_mem_c_t* rw_mem_to_c_type(); \
    \
    /* Obtain const C pointer from object. */ \
    const rw_mem_c_t* rw_mem_to_c_type() const; \
    \
  protected: \
    /* Disallow heap allocation except from inside class. ATTN: Need to do this? */ \
    \
    /* Special operator new to allocate CF tracking data before object. */ \
    void* operator new(size_t size); \
    \
    /* Special operator delete. ATTN: should verify retain count is 0 or 1? */ \
    void operator delete(void *ptr); \
    \
  private: \
    \
    /* CF registration function, called once, the first time the type is allocated. */ \
    static void rw_mem_cf_register(); \
    \
    /* Callback from CF to delete object; calls the destructor in place. */ \
    static void rw_mem_cf_finalize(CFTypeRef cf_obj); \
    \
    /* CF description callback */ \
    static CFStringRef rw_mem_cf_formatting_desc(CFTypeRef cf_obj, CFDictionaryRef formatOpts); \
    \
    /* CF debug description callback */ \
    static CFStringRef rw_mem_cf_debug_desc(CFTypeRef cf_obj); \
    \
    /* CF type ID cannot be in the class, because pure-C code needs to find the global variable */ \
    \
    /* CF runtime type descriptor */ \
    static const CFRuntimeClass rw_mem_cf_runtime_;


#define _RW_CF_TYPE_CLASS_DEFINE_INLINES(c_t, ptr_t, cpp_t) \
  inline cpp_t::suptr_t cpp_t::rw_mem_dupref() \
  { \
    /* duplicate the reference, returning it in a uniquish-pointer */ \
    rw_mem_retain(); \
    cpp_t* me = this; \
    return suptr_t(std::move(me)); \
  } \
  inline void cpp_t::rw_mem_validate(const rw_mem_c_t* c_obj, const char* func) \
  { \
    __CFGenericValidateType_(c_obj, _RW_CF_TYPE_ID(ptr_t), func); \
  } \
  inline void cpp_t::rw_mem_validate(const char* func) const \
  { \
    __CFGenericValidateType_(rw_mem_to_c_type(), _RW_CF_TYPE_ID(ptr_t), func); \
  } \
  inline cpp_t* cpp_t::rw_mem_cf_to_cpp_type(void* cf_obj) \
  { \
    rw_mem_c_t* c_obj = static_cast<rw_mem_c_t*>(cf_obj); \
    if (c_obj) { \
      rw_mem_validate(c_obj, __PRETTY_FUNCTION__); \
    } \
    return rw_mem_to_cpp_type(c_obj); \
  } \
  inline const cpp_t* cpp_t::rw_mem_cf_to_cpp_type(CFTypeRef cf_obj) \
  { \
    const rw_mem_c_t* c_obj = static_cast<const rw_mem_c_t*>(cf_obj); \
    if (c_obj) { \
      rw_mem_validate(c_obj, __PRETTY_FUNCTION__); \
    } \
    return rw_mem_to_cpp_type(c_obj); \
  } \
  inline cpp_t* cpp_t::rw_mem_to_cpp_type(rw_mem_c_t* c_obj) \
  { \
    if (c_obj == nullptr) { \
      return nullptr; \
    } \
    return reinterpret_cast<cpp_t*>( \
      &static_cast<rw_mem_cf_t*>(c_obj)->cpp_base_); \
  } \
  inline const cpp_t* cpp_t::rw_mem_to_cpp_type(const rw_mem_c_t* c_obj) \
  { \
    if (c_obj == nullptr) { \
      return nullptr; \
    } \
    return reinterpret_cast<const cpp_t*>( \
      &static_cast<const rw_mem_cf_t*>(c_obj)->cpp_base_); \
  } \
  inline c_t* cpp_t::rw_mem_to_c_type() \
  { \
    return reinterpret_cast<rw_mem_cf_t*>( \
      reinterpret_cast<char*>(this) - offsetof(rw_mem_cf_t,cpp_base_)); \
  } \
  inline const c_t* cpp_t::rw_mem_to_c_type() const \
  { \
    return reinterpret_cast<const rw_mem_cf_t*>( \
      reinterpret_cast<const char*>(this) - offsetof(rw_mem_cf_t,cpp_base_)); \
  }

#define _RW_CF_TYPE_CLASS_DEFINE_TRAITS(c_t, ptr_t, cpp_t) \
  /* Validate helper type */ \
  template<> \
  struct rw_mem_traits<c_t*> \
  { \
    typedef cpp_t rw_mem_cpp_t; \
    typedef cpp_t* ret_t; \
  }; \
  template<> \
  struct rw_mem_traits<const c_t*> \
  { \
    typedef cpp_t rw_mem_cpp_t; \
    typedef const cpp_t* ret_t; \
  };


#define _RW_CF_TYPE_CLASS_DEFINE_MEMBERS(descr, c_t, ptr_t, cpp_t) \
  CFTypeID _RW_CF_TYPE_ID(ptr_t) = _kCFRuntimeNotATypeID; \
  const CFRuntimeClass cpp_t::rw_mem_cf_runtime_ = \
  { \
    _kCFRuntimeScannedObject, \
    _RW_CF_TYPE_DESCRIPTION(ptr_t), \
    NULL, /* init */ \
    NULL, /* copy */ \
    &cpp_t::rw_mem_cf_finalize, /* finalize */ \
    NULL, /* equal */ \
    NULL, /* hash */ \
    &cpp_t::rw_mem_cf_formatting_desc, /* copyFormattingDesc */ \
    &cpp_t::rw_mem_cf_debug_desc /* copyDebugDesc */ \
  }; \
  void* cpp_t::operator new(size_t size) \
  { \
    static std::once_flag once; \
    std::call_once(once, cpp_t::rw_mem_cf_register); \
    size += sizeof(CFRuntimeBase); \
    void *storage = rw_malloc0_cf_magic(size, &_RW_CF_TYPE_ID(ptr_t), RW_HASH_TYPE(ptr_t), RW_QUOTE(ptr_t), G_STRLOC); \
    if (storage == nullptr) { \
      RW_CRASH(); \
    } \
    return static_cast<char*>(storage)+sizeof(CFRuntimeBase); \
  } \
  void cpp_t::operator delete(void* cf_obj) \
  { \
    /* ATTN: This is fishy */ \
    cpp_t* cpp_obj = rw_mem_cf_to_cpp_type(cf_obj); \
    cpp_obj->~cpp_t(); \
  } \
  void cpp_t::rw_mem_retain() const \
  { \
    CFRetain(rw_mem_to_c_type()); \
  } \
  void cpp_t::rw_mem_release() const \
  { \
    CFRelease(rw_mem_to_c_type()); \
  } \
  void cpp_t::rw_mem_cf_finalize(CFTypeRef cf_obj) \
  { \
    rw_mem_cf_to_cpp_type(cf_obj)->~cpp_t(); \
    /* free() happens in caller */ \
  } \
  void cpp_t::rw_mem_cf_register() \
  { \
    RW_ASSERT(_kCFRuntimeNotATypeID == _RW_CF_TYPE_ID(ptr_t)); \
    _RW_CF_TYPE_ID(ptr_t) = _CFRuntimeRegisterClass(&rw_mem_cf_runtime_); \
  } \
  CFStringRef cpp_t::rw_mem_cf_formatting_desc(CFTypeRef cf_obj, CFDictionaryRef formatOpts) \
  { \
    UNUSED(cf_obj); \
    UNUSED(formatOpts); \
    /* ATTN: Use different allocator?  Define virtual for overload? */ \
    return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR(descr)); \
  } \
  CFStringRef cpp_t::rw_mem_cf_debug_desc(CFTypeRef cf_obj) \
  { \
    UNUSED(cf_obj); \
    /* ATTN: Use different allocator?  Define virtual for overload? */ \
    return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR(descr)); \
  }

#define _RW_CF_TYPE_OBJ_VALIDATE(cpp_obj) \
  rw_mem_validate_impl(cpp_obj, __PRETTY_FUNCTION__)

#define _RW_CF_TYPE_CLASS_VALIDATE_TYPE(cf_obj, cpp_t) \
  cpp_t::rw_mem_validate(cf_obj, __PRETTY_FUNCTION__)


///////////////////////////////////////////////////////////////////////////////

#define RW_CF_TYPE_CLASS_DECLARE_MEMBERS(c_t, ptr_t, cpp_t) \
  _RW_CF_TYPE_CLASS_DECLARE_MEMBERS(c_t, ptr_t, cpp_t)

#define RW_CF_TYPE_CLASS_DEFINE_MEMBERS(descr, c_t, ptr_t, cpp_t) \
  _RW_CF_TYPE_CLASS_DEFINE_MEMBERS(descr, c_t, ptr_t, cpp_t)

#define RW_CF_TYPE_CLASS_DEFINE_INLINES(c_t, ptr_t, cpp_t) \
  _RW_CF_TYPE_CLASS_DEFINE_INLINES(c_t, ptr_t, cpp_t)

#define RW_CF_TYPE_CLASS_DEFINE_TRAITS(c_t, ptr_t, cpp_t) \
  _RW_CF_TYPE_CLASS_DEFINE_TRAITS(c_t, ptr_t, cpp_t)

#define RW_MEM_OBJECT_VALIDATE(cpp_obj) \
  _RW_CF_TYPE_OBJ_VALIDATE(cpp_obj)

#define RW_MEM_CLASS_VALIDATE(cf_obj, cpp_t) \
  _RW_CF_TYPE_CLASS_VALIDATE_TYPE(cf_obj, cpp_t)


/**
 * Object deleter template for types with rw_mem_retain() and
 * rw_mem_release() member functions.  Important for tracking
 * references the C++ way (RAII).
 *
 * ATTN: Probably belongs elsewhere, particularly if we also need to
 * eventually support some kind of GObject-based tracking.  Any such
 * tracking would be implemented in terms of the same member function
 * names.
 *
 * ATTN: TGS: This template is a hack.  It hijacks std::unique_ptr<> to
 * get the full move-semanitcs and automatic management interface of
 * unique_ptr, basically for free, while co-opting it for use with
 * technically shared pointers.  On the plus side, this hack saved me a
 * ton of work, getting me 90% of the functionality I want, basically
 * for free.  On the minus side, I base this on unique_ptr<>, which
 * really wants to own the thing.  Obviously, unique_ptr<> doesn't own
 * the thing, and it makes it a little harder (and non-intuitive) to to
 * actually share pointers.  We should fix those problems someday, but
 * for now this works.
 */
template <class pointer_type_t>
struct UniquishPtrRwMemoryTracking
{
  void operator()(pointer_type_t* obj) const
  {
    obj->rw_mem_release();
  }
  typedef std::unique_ptr<pointer_type_t,UniquishPtrRwMemoryTracking> suptr_t;
};

/**
 * Null validation helper traits class.  This is the terminal "rule"
 * for the SFINAE chain in the validation helpers.  If you end up in
 * this traits class, you'll fail to compile with a bunch of very ugly
 * errors - probably because you typoed something.
 */
template<class t>
struct rw_mem_traits
{};

/**
 * Template function to validate a memory-tracked C++ object, using
 * it's C type.  Blows up on nullptr.
 */
template <
  class ptr_t,
  class cpp_t=typename rw_mem_traits<ptr_t>::rw_mem_cpp_t>
void rw_mem_validate_impl(const ptr_t& p, const char* func)
{
  cpp_t::rw_mem_validate(p, func);
}

/**
 * Template function to validate a memory-tracked C++ object, using
 * it's C type.  Blows up on nullptr.
 */
template <
  class ptr_t,
  class cpp_t=typename std::remove_pointer<ptr_t>::type,
  class c_t=typename cpp_t::rw_mem_c_t>
void rw_mem_validate_impl(const ptr_t& p, const char* func)
{
  RW_ASSERT(p);
  p->rw_mem_validate(func);
}

/**
 * Template function to validate a memory-tracked C++ object, using
 * it's C type.  Blows up on nullptr.
 */
template <
  class ptr_t,
  class cpp_t=typename ptr_t::element_type,
  class suptr_t=typename cpp_t::suptr_t,
  class c_t=typename cpp_t::rw_mem_c_t>
void rw_mem_validate_impl(const ptr_t& p, const char* func)
{
  RW_ASSERT(p);
  p->rw_mem_validate(func);
}

#define RW_MEM_ASSERT_VALID(p) rw_mem_validate(p)
#define RW_MEM_ASSERT_VALID_OR_NULL(p) ( (nullptr==(p)) ? 1 : (rw_mem_validate(p),1) )

/**
 * Casting operator for C to C++ type.
 */
template <
  class ptr_t,
  class traits_t=rw_mem_traits<ptr_t>,
  class cpp_t=typename traits_t::rw_mem_cpp_t,
  class ret_t=typename traits_t::ret_t>
ret_t rw_mem_to_cpp_type(ptr_t p)
{
  return cpp_t::rw_mem_to_cpp_type(p);
}

/**
 * Casting operator for C to C++ type.
 */
template <class ptr_t>
auto rw_mem_to_c_type(ptr_t p) -> decltype(p->rw_mem_to_c_type())
{
  return p->rw_mem_to_c_type();
}

#endif /* __cplusplus */

#endif /* __RW_CF_TYPE_VALIDATE_HPP__ */
