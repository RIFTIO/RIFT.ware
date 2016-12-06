#include <CoreFoundation/CoreFoundation.h>

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
    context.info = malloc(sizeof(int));
    allocator = CFAllocatorCreate(kCFAllocatorUseContext, &context);
  }
  return allocator;
}

#define kNumFamilyMembers 5
void main () {
    CFStringRef names[kNumFamilyMembers];
    CFArrayRef  array;
    CFDataRef   xmlData;
    CFRange range = {0, kNumFamilyMembers};
    char name[100];
    const void *ret_val = NULL;
    CFAllocatorRef allocator = myAllocator();
    // Define the family members.
    names[0] = CFSTR("Marge");
    names[1] = CFSTR("Homer");
    names[2] = CFSTR("Bart");
    names[3] = CFSTR("Lisa");
    names[4] = CFSTR("Maggie");
    // Create a property list using the string array of names.
    array = CFArrayCreate(myAllocator(),
                (const void **)names,
                kNumFamilyMembers,
                &kCFTypeArrayCallBacks );
    printf("Array size = %d\n", CFArrayGetCount(array));
    printf("Array has %s %d times\n", "Marge", CFArrayGetCountOfValue(array, range, CFSTR("Marge")));

    printf("\nCFShow:\n");
    CFShow((CFTypeRef)allocator);
    printf("\n");

    printf("\nCFShow:\n");
    CFShow((CFTypeRef)array);
    printf("\n");

    printf("\nCFShow:\n");
    CFShow((CFTypeRef)CFArrayGetValueAtIndex(array, 3));
    printf("\n");

    printf("\n\nCFShowStr:");
    CFShowStr(CFArrayGetValueAtIndex(array, 3));
    printf("\n");

    ret_val = CFArrayGetValueAtIndex(array, 3);
    CFStringGetCString(ret_val, name, sizeof(name), 0);
    printf("Value at index %d is 0x%x %s\n", 3, ret_val, name);
    // Convert the plist into XML data.
    xmlData = CFPropertyListCreateXMLData( myAllocator(), array );
    // Clean up CF types.
    CFRelease( array );
    CFRelease( xmlData );
}
