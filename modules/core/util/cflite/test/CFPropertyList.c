#include <CoreFoundation/CoreFoundation.h>
#define kNumKids 2
#define kNumBytesInPic 10
CFDictionaryRef CreateMyDictionary( void );
CFPropertyListRef CreateMyPropertyListFromFile( CFURLRef fileURL );
void WriteMyPropertyListToFile( CFPropertyListRef propertyList,
            CFURLRef fileURL );
int main () {
   CFPropertyListRef propertyList;
   CFURLRef fileURL;
   // Construct a complex dictionary object;
   propertyList = CreateMyDictionary();
   // Create a URL that specifies the file we will create to
   // hold the XML data.
   fileURL = CFURLCreateWithFileSystemPath( kCFAllocatorDefault,
              CFSTR("test.txt"),
              kCFURLPOSIXPathStyle,
              false );
// file path name
// interpret as POSIX path
// is it a directory?
   // Write the property list to the file.
   WriteMyPropertyListToFile( propertyList, fileURL );
   CFRelease(propertyList);
   // Recreate the property list from the file.
   propertyList = CreateMyPropertyListFromFile( fileURL );
   // Release any objects to which we have references.
   CFRelease(propertyList);
   CFRelease(fileURL);
   return 0;
}

CFDictionaryRef CreateMyDictionary( void ) {
  CFMutableDictionaryRef dict;
  CFNumberRef num;
  CFArrayRef array;
  CFDataRef data;
  int yearOfBirth;
  CFStringRef kidsNames[kNumKids];

  // Fake data to stand in for a picture of John Doe.
  const unsigned char pic[kNumBytesInPic] = {0x3c, 0x42, 0x81,
         0xa5, 0x81, 0xa5, 0x99, 0x81, 0x42, 0x3c};
  // Define some data.
  kidsNames[0] = CFSTR("John");
  kidsNames[1] = CFSTR("Kyra");
  yearOfBirth = 1965;
  // Create a dictionary that will hold the data.
  dict = CFDictionaryCreateMutable( kCFAllocatorDefault,
           0,
           &kCFTypeDictionaryKeyCallBacks,
           &kCFTypeDictionaryValueCallBacks );
  // Put the various items into the dictionary.
  // Because the values are retained as they are placed into the
  //  dictionary, we can release any allocated objects here.
  CFDictionarySetValue( dict, CFSTR("Name"), CFSTR("John Doe") );
  CFDictionarySetValue( dict,
          CFSTR("City of Birth"),
          CFSTR("Springfield") );
  num = CFNumberCreate( kCFAllocatorDefault,
          kCFNumberIntType,
          &yearOfBirth );
  CFDictionarySetValue( dict, CFSTR("Year Of Birth"), num );
  CFRelease( num );
  array = CFArrayCreate( kCFAllocatorDefault,
              (const void **)kidsNames,
              kNumKids,
              &kCFTypeArrayCallBacks );
  CFDictionarySetValue( dict, CFSTR("Kids Names"), array );
  CFRelease( array );
  array = CFArrayCreate( kCFAllocatorDefault,
                NULL,
                0,
                &kCFTypeArrayCallBacks );
  CFDictionarySetValue( dict, CFSTR("Pets Names"), array );
  CFRelease( array );
  data = CFDataCreate( kCFAllocatorDefault, pic, kNumBytesInPic );
  CFDictionarySetValue( dict, CFSTR("Picture"), data );
  CFRelease( data );
  return dict;
}

void WriteMyPropertyListToFile( CFPropertyListRef propertyList,
            CFURLRef fileURL ) {
   CFDataRef xmlData;
   Boolean status;
   SInt32 errorCode;
   // Convert the property list into XML data.
   xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, propertyList );
   // Write the XML data to the file.
   status = CFURLWriteDataAndPropertiesToResource (
               fileURL,
               xmlData,
               NULL,
               &errorCode);
   CFRelease(xmlData);
}
// URL to use
// data to write
CFPropertyListRef CreateMyPropertyListFromFile( CFURLRef fileURL ) {
   CFPropertyListRef propertyList;
   CFStringRef       errorString;
   CFDataRef         resourceData;
   Boolean           status;
   SInt32            errorCode;
   // Read the XML file.
   status = CFURLCreateDataAndPropertiesFromResource(
              kCFAllocatorDefault,
              fileURL,
              &resourceData,
              NULL,
              NULL,
              &errorCode);
// place to put file data
   // Reconstitute the dictionary using the XML data.
   propertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
               resourceData,
               kCFPropertyListImmutable,
               &errorString);
   CFRelease( resourceData );
   return propertyList;
}
