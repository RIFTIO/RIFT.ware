#if !defined(__MYCFTYPE02_H__)
#define __MYCFTYPE02_H__

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDictionary.h>
#include <stddef.h>

CF_EXTERN_C_BEGIN
// ========================= EXAMPLE =========================

// Example: MyCFType -- a "range" object, which keeps the starting
//       location and length of the range. ("EX" as in "EXample").

// ---- API ----

typedef const struct __MyCFType * MyCFTypeRef;

CFTypeID MyCFTypeGetTypeID(void);

MyCFTypeRef MyCFTypeCreate(CFAllocatorRef allocator, uint32_t location, uint32_t length);

uint32_t MyCFTypeGetLocation(MyCFTypeRef rangeref);
uint32_t MyCFTypeGetLength(MyCFTypeRef rangeref);

CF_EXTERN_C_END
#endif /* !defined(__MYCFTYPE02_H__) */
