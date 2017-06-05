
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ipsec_state.pb-c.c"
#include "ipsec_state.pb2c.c"
#include "ipsec_state_flat_cstructs.c"



#include "ex1.pb-c.c"
#include "ex1.pb2c.c"


#include "ex2_base1.pb-c.c"
#include "ex2_base3.pb-c.c"
#include "ex2_base2.pb-c.c"
#include "ex2_base4.pb-c.c"
#include "ex2.pb-c.c"
#include "ex2_base1.pb2c.c"
#include "ex2_base3.pb2c.c"
#include "ex2_base2.pb2c.c"
#include "ex2_base4.pb2c.c"
#include "ex2.pb2c.c"

#include "ex3.pb-c.c"

#define TIM_FLAT
#ifdef TIM_FLAT
#include "tim.pb-c.c"
#include "tim_flat3_cstructs.c"
#else
#include "tim.pb-c.c"
#include "tim.pb2c.c"
#endif


#include "sample1.pb-c.c"
// "normal" generated C types
#include "sample1.pb2c.c"
// "flattened" generated C types
#include "sample1_flat_cstructs.c"


