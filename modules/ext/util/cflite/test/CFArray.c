#include <CoreFoundation/CoreFoundation.h>
#define kNumFamilyMembers 5
void main () {
    CFStringRef names[kNumFamilyMembers];
    CFArrayRef  array;
    CFDataRef   xmlData;
    CFRange range = {0, kNumFamilyMembers};
    char name[100];
    const void *ret_val = NULL;
    // Define the family members.
    names[0] = CFSTR("Marge");
    names[1] = CFSTR("Homer");
    names[2] = CFSTR("Bart");
    names[3] = CFSTR("Lisa");
    names[4] = CFSTR("Maggie");
    // Create a property list using the string array of names.
    array = CFArrayCreate( kCFAllocatorDefault,
                (const void **)names,
                kNumFamilyMembers,
                &kCFTypeArrayCallBacks );
    printf("Array size = %d\n", CFArrayGetCount(array));
    printf("Array has %s %d times\n", "Marge", CFArrayGetCountOfValue(array, range, CFSTR("Marge")));

    printf("\nCFShow:\n");
    CFShow((CFTypeRef)CFArrayGetValueAtIndex(array, 3));
    printf("\n\nCFShowStr:");
    CFShowStr(CFArrayGetValueAtIndex(array, 3));
    printf("\n");

    ret_val = CFArrayGetValueAtIndex(array, 3);
    CFStringGetCString(ret_val, name, sizeof(name), 0);
    printf("Value at index %d is 0x%x %s\n", 3, ret_val, name);
    // Convert the plist into XML data.
    xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, array );
    // Clean up CF types.
    CFRelease( array );
    CFRelease( xmlData );
}
