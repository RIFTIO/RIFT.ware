#define BOOST_TEST_MODULE IntegTestSimpleEditStartupFalse

#include "configure-yuma-integtest.h"

namespace YumaTest {

// ---------------------------------------------------------------------------|
// Initialise the spoofed command line arguments 
// ---------------------------------------------------------------------------|
const char* SpoofedArgs::argv[] = {
    ( "yuma-test" ),
    ( "--modpath=../../modules/netconfcentral"
               ":../../modules/ietf"
               ":../../modules/yang"
               ":../modules/yang"
               ":../../modules/test/pass" ),
    ( "--runpath=../modules/sil" ),
    ( "--access-control=off" ),
    ( "--log=./yuma-op/yuma-out.txt" ),
    ( "--log-level=debug4" ),
    ( "--target=candidate" ),
    ( "--module=simple_list_test" ),
    ( "--with-startup=false" ), 
};

#include "define-yuma-integtest-global-fixture.h"

} // namespace YumaTest
