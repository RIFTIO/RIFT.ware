#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include "myCFType02.h"

struct __MyCFType {
    CFRuntimeBase _base;
    uint32_t _location;
    uint32_t _length;
};

static Boolean __MyCFTypeEqual(CFTypeRef cf1, CFTypeRef cf2) {
    MyCFTypeRef rangeref1 = (MyCFTypeRef)cf1;
    MyCFTypeRef rangeref2 = (MyCFTypeRef)cf2;
    if (rangeref1->_location != rangeref2->_location) return false;
    if (rangeref1->_length != rangeref2->_length) return false;
    return true;
}

static CFHashCode __MyCFTypeHash(CFTypeRef cf) {
    MyCFTypeRef rangeref = (MyCFTypeRef)cf;
    return (CFHashCode)(rangeref->_location + rangeref->_length);
}

static CFStringRef __MyCFTypeCopyFormattingDesc(CFTypeRef cf, CFDictionaryRef formatOpts) {
    MyCFTypeRef rangeref = (MyCFTypeRef)cf;
    return CFStringCreateWithFormat(CFGetAllocator(rangeref), formatOpts,
		CFSTR("[%u, %u)"),
		rangeref->_location,
		rangeref->_location + rangeref->_length);
}

static CFStringRef __MyCFTypeCopyDebugDesc(CFTypeRef cf) {
    MyCFTypeRef rangeref = (MyCFTypeRef)cf;
    return CFStringCreateWithFormat(CFGetAllocator(rangeref), NULL,
		CFSTR("<MyCFType %p [%p]>{loc = %u, len = %u}"),
		rangeref,
		CFGetAllocator(rangeref),
		rangeref->_location,
		rangeref->_length);
}

static void __MyCFTypeMyCFTypeFinalize(CFTypeRef cf) {
    MyCFTypeRef rangeref = (MyCFTypeRef)cf;
    // nothing to finalize
}

static CFTypeID _kMyCFTypeID = _kCFRuntimeNotATypeID;

static CFRuntimeClass _kMyCFTypeClass = {0};

/* Something external to this file is assumed to call this
 * before the MyCFType class is used.
 */
void __MyCFTypeClassInitialize(void) {
    _kMyCFTypeClass.version = 0;
    _kMyCFTypeClass.className = "MyCFType";
    _kMyCFTypeClass.init = NULL;
    _kMyCFTypeClass.copy = NULL;
    _kMyCFTypeClass.finalize = __MyCFTypeMyCFTypeFinalize;
    _kMyCFTypeClass.equal = __MyCFTypeEqual;
    _kMyCFTypeClass.hash = __MyCFTypeHash;
    _kMyCFTypeClass.copyFormattingDesc = __MyCFTypeCopyFormattingDesc;
    _kMyCFTypeClass.copyDebugDesc = __MyCFTypeCopyDebugDesc;
    _kMyCFTypeID = _CFRuntimeRegisterClass((const CFRuntimeClass * const)&_kMyCFTypeClass);
}

CFTypeID MyCFTypeGetTypeID(void) {
    return _kMyCFTypeID;
}

MyCFTypeRef MyCFTypeCreate(CFAllocatorRef allocator, uint32_t location, uint32_t length) {
    struct __MyCFType *newrange;
    uint32_t extra = sizeof(struct __MyCFType) - sizeof(CFRuntimeBase);
    newrange = (struct __MyCFType *)_CFRuntimeCreateInstance(allocator, _kMyCFTypeID, extra, NULL);
    if (NULL == newrange) {
	return NULL;
    }
    newrange->_location = location;
    newrange->_length = length;
    return (MyCFTypeRef)newrange;
}

uint32_t MyCFTypeGetLocation(MyCFTypeRef rangeref) {
    return rangeref->_location;
}

uint32_t MyCFTypeGetLength(MyCFTypeRef rangeref) {
    return rangeref->_length;
}
