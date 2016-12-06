
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
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


