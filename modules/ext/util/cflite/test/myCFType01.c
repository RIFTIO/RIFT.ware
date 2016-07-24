#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include "myCFType01.h"

struct __EXRange {
    CFRuntimeBase _base;
    uint32_t _location;
    uint32_t _length;
};

static Boolean __EXRangeEqual(CFTypeRef cf1, CFTypeRef cf2) {
    EXRangeRef rangeref1 = (EXRangeRef)cf1;
    EXRangeRef rangeref2 = (EXRangeRef)cf2;
    if (rangeref1->_location != rangeref2->_location) return false;
    if (rangeref1->_length != rangeref2->_length) return false;
    return true;
}

static CFHashCode __EXRangeHash(CFTypeRef cf) {
    EXRangeRef rangeref = (EXRangeRef)cf;
    return (CFHashCode)(rangeref->_location + rangeref->_length);
}

static CFStringRef __EXRangeCopyFormattingDesc(CFTypeRef cf, CFDictionaryRef formatOpts) {
    EXRangeRef rangeref = (EXRangeRef)cf;
    return CFStringCreateWithFormat(CFGetAllocator(rangeref), formatOpts,
		CFSTR("[%u, %u)"),
		rangeref->_location,
		rangeref->_location + rangeref->_length);
}

static CFStringRef __EXRangeCopyDebugDesc(CFTypeRef cf) {
    EXRangeRef rangeref = (EXRangeRef)cf;
    return CFStringCreateWithFormat(CFGetAllocator(rangeref), NULL,
		CFSTR("<EXRange %p [%p]>{loc = %u, len = %u}"),
		rangeref,
		CFGetAllocator(rangeref),
		rangeref->_location,
		rangeref->_length);
}

static void __EXRangeEXRangeFinalize(CFTypeRef cf) {
    EXRangeRef rangeref = (EXRangeRef)cf;
    // nothing to finalize
}

static CFTypeID _kEXRangeID = _kCFRuntimeNotATypeID;

static CFRuntimeClass _kEXRangeClass = {0};

/* Something external to this file is assumed to call this
 * before the EXRange class is used.
 */
void __EXRangeClassInitialize(void) {
    _kEXRangeClass.version = 0;
    _kEXRangeClass.className = "EXRange";
    _kEXRangeClass.init = NULL;
    _kEXRangeClass.copy = NULL;
    _kEXRangeClass.finalize = __EXRangeEXRangeFinalize;
    _kEXRangeClass.equal = __EXRangeEqual;
    _kEXRangeClass.hash = __EXRangeHash;
    _kEXRangeClass.copyFormattingDesc = __EXRangeCopyFormattingDesc;
    _kEXRangeClass.copyDebugDesc = __EXRangeCopyDebugDesc;
    _kEXRangeID = _CFRuntimeRegisterClass((const CFRuntimeClass * const)&_kEXRangeClass);
}

CFTypeID EXRangeGetTypeID(void) {
    return _kEXRangeID;
}

EXRangeRef EXRangeCreate(CFAllocatorRef allocator, uint32_t location, uint32_t length) {
    struct __EXRange *newrange;
    uint32_t extra = sizeof(struct __EXRange) - sizeof(CFRuntimeBase);
    newrange = (struct __EXRange *)_CFRuntimeCreateInstance(allocator, _kEXRangeID, extra, NULL);
    if (NULL == newrange) {
	return NULL;
    }
    newrange->_location = location;
    newrange->_length = length;
    return (EXRangeRef)newrange;
}

uint32_t EXRangeGetLocation(EXRangeRef rangeref) {
    return rangeref->_location;
}

uint32_t EXRangeGetLength(EXRangeRef rangeref) {
    return rangeref->_length;
}
