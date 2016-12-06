#if !defined(__MYCFTYPE01_H__)
#define __MYCFTYPE01_H__

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDictionary.h>
#include <stddef.h>

CF_EXTERN_C_BEGIN
// ========================= EXAMPLE =========================

// Example: EXRange -- a "range" object, which keeps the starting
//       location and length of the range. ("EX" as in "EXample").

// ---- API ----

typedef const struct __EXRange * EXRangeRef;

CFTypeID EXRangeGetTypeID(void);

EXRangeRef EXRangeCreate(CFAllocatorRef allocator, uint32_t location, uint32_t length);

uint32_t EXRangeGetLocation(EXRangeRef rangeref);
uint32_t EXRangeGetLength(EXRangeRef rangeref);

CF_EXTERN_C_END
#endif /* !defined(__MYCFTYPE01_H__) */
