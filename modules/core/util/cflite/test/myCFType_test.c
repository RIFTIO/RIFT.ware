#include <CoreFoundation/CoreFoundation.h>
#include "myCFType01.h"
#include "myCFType02.h"
extern void __EXRangeClassInitialize(void);

void  myFreeInfo(const void  *info) {
    free((void*)info);
}

void * myAlloc(CFIndex size, CFOptionFlags hint, void *info) {
  return malloc(size);
}

void * myRealloc(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) {
  return realloc(ptr, newsize);
}

void myDealloc(void *ptr, void *info) {
  return free(ptr);
}

CFStringRef myCopyDescription(const void *info) {
  return CFSTR("My myAllocator");
}

CFIndex myPreferredSize ( CFIndex size, CFOptionFlags hint, void *info) {
  return size;
}


static CFAllocatorRef myAllocator(void) {
  static CFAllocatorRef allocator = NULL;
  if (!allocator) {
    CFAllocatorContext context =
     {
        0,                      //CFIndex version;
        NULL,                   //void *info;
        NULL,                   //CFAllocatorRetainCallBack; callback that retains the data pointed to by the info field
        myFreeInfo,             //CFAllocatorReleaseCallBack; callback that releases the data pointed to by the info field
        myCopyDescription,      //CFAllocatorCopyDescriptionCallBack; callback that provides a description of the data pointed to by the info field
        myAlloc,                //CFAllocatorAllocateCallBack; callback that allocates memory of a requested size
        myRealloc,              //CFAllocatorReallocateCallBack; callback that reallocates memory of a requested size for an existing block of memory. 
        myDealloc,              //CFAllocatorDeallocateCallBack; callback that deallocates a given block of memory
        myPreferredSize,        //CFAllocatorPreferredSizeCallBack; callback that determines whether there is enough free memory to satisfy a request
    };
    context.info = malloc(10000);
    allocator = CFAllocatorCreate(kCFAllocatorUseContext, &context);
  }
  return allocator;
}

bool __CFOASafe = true;
void rw_cfRecordAllocationFunction(int i1, void *p1, int64_t i2, uint64_t i3, const char *p2)
{
      printf("%p : alloc called: %s\n", p1, p2);
}

void rw_cfLastAllocEventNameFunction(void *p1, const char *p2)
{
      printf("%p : last alloc called: %s\n", p1, p2);
}

extern  void (*__CFObjectAllocRecordAllocationFunction)(int, void *, int64_t , uint64_t, const char *);
extern  void (*__CFObjectAllocSetLastAllocEventNameFunction)(void *, const char *);

void
main() {
  //EXRangeRef exrange;
  MyCFTypeRef mycf;
  MyCFTypeRef mycf1;
  MyCFTypeRef mycf2;

  //__EXRangeClassInitialize();
  __MyCFTypeClassInitialize();

#if 0
  // Create a CFType
  exrange = EXRangeCreate(kCFAllocatorDefault, 10, 15);

  printf("EXRangeGetTypeID = %u EXRangeGetLocation = %u EXRangeGetLength = %u\n",
         EXRangeGetTypeID(),
         EXRangeGetLocation(exrange),
         EXRangeGetLength(exrange));

    CFShow(exrange);
    printf("\n");
    CFRelease(exrange);
#endif
  void (*tmp_CFObjectAllocSetLastAllocEventNameFunction)(void *, const char *) = __CFObjectAllocSetLastAllocEventNameFunction;
  void (*tmp_CFObjectAllocRecordAllocationFunction)(int, void *, int64_t , uint64_t, const char *) = __CFObjectAllocRecordAllocationFunction;
  __CFObjectAllocSetLastAllocEventNameFunction = rw_cfLastAllocEventNameFunction;
  __CFObjectAllocRecordAllocationFunction = rw_cfRecordAllocationFunction;;
  int i;

  // Create a CFType
  //mycf = MyCFTypeCreate(kCFAllocatorDefault, 10, 15);
  for (i=0; i<2; i++) {
  mycf = MyCFTypeCreate(myAllocator(), 10, 15);

  printf("MyCFTypeGetTypeID = %u MyCFTypeGetLocation = %u MyCFTypeGetLength = %u\n",
         MyCFTypeGetTypeID(),
         MyCFTypeGetLocation(mycf),
         MyCFTypeGetLength(mycf));

  //CFShow(mycf);
  //printf("\n");
  CFRelease(mycf);
  }

  __CFObjectAllocSetLastAllocEventNameFunction = tmp_CFObjectAllocSetLastAllocEventNameFunction;
  __CFObjectAllocRecordAllocationFunction = tmp_CFObjectAllocRecordAllocationFunction;

  /*
  mycf1 = MyCFTypeCreate(myAllocator(), 10, 15);
  CFRelease(mycf1);

  mycf1 = MyCFTypeCreate(myAllocator(), 10, 15);
  CFRelease(mycf1);

  mycf1 = MyCFTypeCreate(myAllocator(), 10, 15);
  CFRelease(mycf1);

  mycf1 = MyCFTypeCreate(myAllocator(), 10, 15);
  CFRelease(mycf1);

  mycf1 = MyCFTypeCreate(myAllocator(), 10, 15);
  CFRelease(mycf1);

  mycf1 = MyCFTypeCreate(myAllocator(), 10, 15);
  CFRelease(mycf1);

  mycf1 = MyCFTypeCreate(myAllocator(), 10, 15);
  CFRelease(mycf1);

  mycf2 = MyCFTypeCreate(myAllocator(), 10, 15);
  CFRelease(mycf2);
  */
}
