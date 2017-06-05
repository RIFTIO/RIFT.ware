
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <rwsched.h>
#include <rwtrace.h>
#include <rwvcs.h>

#include "rwmain.h"

int main(int argc, char ** argv, char ** envp)
{
  struct rwmain_gi * rwmain;

  rwmain = rwmain_setup(argc, argv, envp);
  if (!rwmain)
    return 0;

  // Run a never ending runloop until there is no work left to do
  rwsched_instance_CFRunLoopRun(rwmain->rwvx->rwsched);

  return 0;
}


