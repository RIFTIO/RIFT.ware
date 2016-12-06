#include <CoreFoundation/CoreFoundation.h>
#define kNumFamilyMembers 5

CFComparisonResult my_strcmp(const void *val1, const void *val2, void *context) {
  char nam1[100], nam2[100];
  CFStringGetCString(val1, nam1, sizeof(nam1), 0);
  CFStringGetCString(val2, nam2, sizeof(nam2), 0);
  //printf("%s <=> %s\n", nam1, nam2);
  return 0-strcmp(nam1, nam2);
}

void main () {
    CFStringRef names[kNumFamilyMembers];
    CFMutableArrayRef  m_array;
    CFRange range = {0, kNumFamilyMembers};
    char name[100];
    const void *ret_val = NULL;
    int i;
    // Create a mutable-array m_array
    m_array = CFArrayCreateMutable( kCFAllocatorDefault,
                                   kNumFamilyMembers,
                                   &kCFTypeArrayCallBacks );
    // Define the family members.
    names[0] = CFSTR("Marge");
    names[1] = CFSTR("Homer");
    names[2] = CFSTR("Bart");
    names[3] = CFSTR("Lisa");
    names[4] = CFSTR("Maggie");
    //  Populate m_array of names.
    for (i=0; i<kNumFamilyMembers; i++)
      CFArrayAppendValue(m_array,
                         (const void *)(names[i]));

    printf("\nCFShow:\n");
    CFShow((CFTypeRef)m_array);
    printf("\n");

    printf("Array size = %d\n", CFArrayGetCount(m_array));

    range.length = CFArrayGetCount(m_array);;
    CFArraySortValues(m_array,
                      range,
                      my_strcmp, NULL);

    printf("\nCFShow:\n");
    CFShow((CFTypeRef)m_array);
    printf("\n");

    range.length = CFArrayGetCount(m_array);;
    i = CFArrayBSearchValues(m_array, range, CFSTR("Homer"), my_strcmp, NULL);
    range.length = CFArrayGetCount(m_array);;
    printf("Array has %s %d times, at index %d\n", "Homer",
           CFArrayGetCountOfValue(m_array, range, CFSTR("Homer")),
           i);

    CFArrayExchangeValuesAtIndices(m_array, 0, 3);
    // CFArrayRemoveValueAtIndex: All values in theArray with indices larger than idx have their indices
    // decreased by one.
    CFArrayRemoveValueAtIndex(m_array, 3);

    range.length = CFArrayGetCount(m_array);;
    CFArraySortValues(m_array,
                      range,
                      my_strcmp, NULL);

    printf("Array size = %d\n", CFArrayGetCount(m_array));

    printf("\nCFShow:\n");
    CFShow((CFTypeRef)m_array);
    printf("\n");

    range.length = CFArrayGetCount(m_array);;
    //CFArrayBSearchValues - Return Value
    //The return value is one of the following:
    //
    //The index of a value that matched, if the target value matches one or more
    //in the range.
    //Greater than or equal to the end point of the range, if the value is
    //greater than all the values in the range.
    //The index of the value greater than the target value, if the value lies
    //between two of (or less than all of) the values in the range.
    i = CFArrayBSearchValues(m_array, range, CFSTR("Marge"), my_strcmp, NULL);
    range.length = CFArrayGetCount(m_array);;
    printf("Array has %s %d times, at index %d\n", "Marge",
           CFArrayGetCountOfValue(m_array, range, CFSTR("Marge")),
           i);

    CFArrayInsertValueAtIndex(m_array, 4, CFSTR("Marge"));

    range.length = CFArrayGetCount(m_array);;
    CFArraySortValues(m_array,
                      range,
                      my_strcmp, NULL);

    range.length = CFArrayGetCount(m_array);;
    i = CFArrayBSearchValues(m_array, range, CFSTR("Marge"), my_strcmp, NULL);
    range.length = CFArrayGetCount(m_array);;
    printf("Array has %s %d times, at index %d\n", "Marge",
           CFArrayGetCountOfValue(m_array, range, CFSTR("Marge")),
           i);

    printf("\nCFShow:\n");
    CFShow((CFTypeRef)CFArrayGetValueAtIndex(m_array, 3));
    printf("\n\nCFShowStr:");
    CFShowStr(CFArrayGetValueAtIndex(m_array, 3));
    printf("\n");

    ret_val = CFArrayGetValueAtIndex(m_array, 3);
    CFStringGetCString(ret_val, name, sizeof(name), 0);
    printf("Value at index %d is 0x%x %s\n", 3, ret_val, name);

    // Clean up CF types.
    CFRelease( m_array );
}
