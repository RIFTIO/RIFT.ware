#include <CoreFoundation/CoreFoundation.h>
#include <netinet/in.h>
#define BUFSIZE 2048

void handleBytes(UInt8 *buf, CFIndex bytesRead) {
  int i;
  printf("READ:\n");
  for (i=0; i<bytesRead; i++) {
    printf("%c 0x%x ", buf[i], buf[i]);
  }
}

void reportError(CFStreamError myErr) {
  printf("ERROR: - errno %d\n", myErr.error);
}

void reportCompletion(void) {
  printf("DONE\n");
}

#if 0
void reader () {

    CFStringRef fileURL = = CFSTR("TestFile.txt");
    CFIndex numBytesRead;
    CFReadStreamRef myReadStream = CFReadStreamCreateWithFile(kCFAllocatorDefault, fileURL);
    CFStreamError myErr;

    if (!CFReadStreamOpen(myReadStream)) {
      myErr = CFReadStreamGetError(myReadStream);
      // An error has occurred.
      if (myErr.domain == kCFStreamErrorDomainPOSIX) {
      // Interpret myErr.error as a UNIX errno.
      } else if (myErr.domain == kCFStreamErrorDomainMacOSStatus) {
      // Interpret myErr.error as a MacOS error code.
        OSStatus macError = (OSStatus)myErr.error;
      // Check other error domains.
      }
    }


    do {
      UInt8 buf[BUFSIZE]; // define BUFSIZE as desired
      numBytesRead = CFReadStreamRead(myReadStream, buf, sizeof(buf));
      if( numBytesRead > 0 ) {
          handleBytes(buf, numBytesRead);
      } else if( numBytesRead < 0 ) {
          myErr = CFReadStreamGetError(myReadStream);
          reportError(myErr);
      }
    } while( numBytesRead > 0 );

    CFReadStreamClose(myReadStream);
    CFRelease(myReadStream);
    myReadStream = NULL;
}

void writer () {

    CFStringRef fileURL = = CFSTR("TestFile.txt");
    CFIndex numBytesRead;
    CFWriteStreamRef myWriteStream = CFWriteStreamCreateWithFile(kCFAllocatorDefault, fileURL);
    CFStreamError myErr;

    if (!CFWriteStreamOpen(myWriteStream)) {
      myErr = CFWriteStreamGetError(myWriteStream);
    // An error has occurred.
      if (myErr.domain == kCFStreamErrorDomainPOSIX) {
      // Interpret myErr.error as a UNIX errno.
      } else if (myErr.domain == kCFStreamErrorDomainMacOSStatus) {
          // Interpret myErr.error as a MacOS error code.
          OSStatus macError = (OSStatus)myErr.error;
          // Check other error domains.
      } 
    }

    UInt8 buf[] = "Hello, world";
    CFIndex bufLen = (CFIndex)strlen(buf);

    while (!done) {
      CFIndex bytesWritten = CFWriteStreamWrite(myWriteStream, buf, (CFIndex)bufLen);
      if (bytesWritten < 0) {
          myErr = CFWriteStreamGetError(myWriteStream);
          reportError(myErr);
      } else if (bytesWritten == 0) {
          if (CFWriteStreamGetStatus(myWriteStream) == kCFStreamStatusAtEnd) { done = TRUE; }
      } else if (bytesWritten != bufLen) {
          // Determine how much has been written and adjust the buffer
          bufLen = bufLen - bytesWritten;
          memmove(buf, buf + bytesWritten, bufLen);
          // Figure out what went wrong with the write stream
          myErr = CFWriteStreamGetError(myWriteStream);
          reportError(myErr);
      }
    }
    CFWriteStreamClose(myWriteStream);
    CFRelease(myWriteStream);
    myWriteStream = NULL;
}
#endif

void myCallBack (CFReadStreamRef stream, CFStreamEventType event, void *myPtr) {
      switch(event) {
      char buf[BUFSIZE];
      CFIndex bytesRead;
      CFStreamError error;
          case kCFStreamEventHasBytesAvailable:
              bytesRead = CFReadStreamRead(stream, buf, BUFSIZE);
              // It is safe to call CFReadStreamRead; it won't block because bytes
              // are available.
              if (bytesRead > 0) {
                  handleBytes(buf, bytesRead);
              }
              // It is safe to ignore a value of bytesRead that is less than or
              // equal to zero because these cases will generate other events.
              break;
          case kCFStreamEventErrorOccurred:
              error = CFReadStreamGetError(stream);
              reportError(error);
              CFReadStreamUnscheduleFromRunLoop(stream, CFRunLoopGetCurrent(),
                                                kCFRunLoopCommonModes);
              CFReadStreamClose(stream);
              CFRelease(stream);
              break;
          case kCFStreamEventEndEncountered:
              reportCompletion();
              CFReadStreamUnscheduleFromRunLoop(stream, CFRunLoopGetCurrent(),
                                                kCFRunLoopCommonModes);
              CFReadStreamClose(stream);
              CFRelease(stream);
              break;
      }
}


main() {
  CFReadStreamRef myReadStream;
  CFReadStreamRef myWriteStream;
  UInt8 *myPtr = NULL;
  void *(*myRetain)(void *info) = NULL;
  // if you set it to your function myRetain, as in the code above, CFStream will call myRetain(myPtr)
  // to retain the info pointer */
  void *(*myRelease)(void *info) = NULL;
  //the release parameter, myRelease, is a pointer to a function to release the info parameter.
  //When the stream is disassociated from the context, CFStream would call myRelease(myPtr)
  CFStringRef (*myCopyDesc)(void *info) = NULL;
  //copyDescription is a parameter to a function to provide a description of the stream. For example,
  //if you were to call CFCopyDesc(myReadStream) with the stream client context shown above,
  //CFStream would call myCopyDesc(myPtr).

  CFStreamCreatePairWithSocketToHost(kCFAllocatorDefault, CFSTR("127.0.0.1"), 1234, &myReadStream, &myWriteStream);

  CFStreamClientContext myContext = {0, myPtr, myRetain, myRelease, myCopyDesc};

  CFOptionFlags registeredEvents = kCFStreamEventHasBytesAvailable
                                 | kCFStreamEventErrorOccurred
                                 | kCFStreamEventEndEncountered;
  if (CFReadStreamSetClient(myReadStream, registeredEvents, myCallBack, &myContext)) {
    CFReadStreamScheduleWithRunLoop(myReadStream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
  }

  if (!CFReadStreamOpen(myReadStream)) {
    CFStreamError myErr = CFReadStreamGetError(myReadStream);
    if (myErr.error != 0) {
    // An error has occurred.
        if (myErr.domain == kCFStreamErrorDomainPOSIX) {
        // Interpret myErr.error as a UNIX errno.
            strerror(myErr.error);
        } else if (myErr.domain == kCFStreamErrorDomainMacOSStatus) {
            OSStatus macError = (OSStatus)myErr.error;
            }
        // Check other domains.
    } else
        // start the run loop
        CFRunLoopRun();
  }
}

//{
  //struct sockaddr_in ip4addr; // note that this only works for ipv4, for ipv6 you need struct sockaddr_in6.

  //ip4addr.sin_family = AF_INET;
  //ip4addr.sin_port = htons(3490);
  //inet_pton(AF_INET, "127.0.0.1", &ip4addr.sin_addr);

  //CFDataRef sockData = CFDataCreate(NULL, (const UInt8 *)&ip4addr, sizeof(ip4addr));
  //CFHostRef host = CFHostCreateWithAddress(NULL, sockData);
  // use 'host' to create your stream   
  //SInt32 port = 1234;

  //CFStreamCreatePairWithSocketToCFHost(kCFAllocatorDefault, host, port, &myReadStream, &myWriteStream);

  //CFRelease(host);
  //CFRelease(sockData);
//}
