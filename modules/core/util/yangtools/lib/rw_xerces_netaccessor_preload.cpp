// This source file is a workaround for RIFT-11693.  The root cause is that
// both Automation code and Xerces (via Protobuf-GI from_xml() call), end up calling
// SSL_library_init() which causes context allocation conflicts.  In order
// to prevent this from occuring without re-forking xerces, we use a preload
// to prevent curl from being initialized since it is not being used anyways.
//
// This file is meant to be compiled into a shared library which can then
// be added to the LD_PRELOAD path.
//    for example with "g++ -g -Wall -shared -fPIC foo.cpp -o foo.so".
//
// set LD_PRELOAD to the path of your new shared library and call the
// program in which you want to intercept something. in bash, you can
// do it like this: "LD_PRELOAD=./foo.so /usr/bin/some_program".

namespace xercesc_3_1{
class CurlNetAccessor
{
public:
  void initCurl(void);
};

void CurlNetAccessor::initCurl(void) {return;}
}
