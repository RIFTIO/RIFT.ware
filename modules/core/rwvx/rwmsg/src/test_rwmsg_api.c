
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
 */


/**
 * @file test_rwmsg_api.c
 * @author RIFT.io <info@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG test code
 */

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/types.h>

#include <unistd.h>
#include <sys/syscall.h>

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>

#include <rwlib.h>
#include <rw_rand.h>
#include <rwtrace.h>

#include "rwmsg.h"

#include "test1.pb-c.h"

/* The code, it does nothing useful!  But it invokes all of the API to
   verify compiling/linking everything as C. */

static void __attribute__((unused)) rwmsg_unused(void) {
  rwsched_instance_ptr_t rws=NULL;
  rwsched_tasklet_ptr_t tinfo=NULL;
  rwmsg_endpoint_t *ep;
  rwtrace_ctx_t *rwtctx = rwtrace_init();
  ep = rwmsg_endpoint_create(1, 0, 0, rws, tinfo, rwtctx, NULL);       ep=ep;
}

int main(int argc, char **argv, char **envp) {
  exit(0);
  argc=argc;
  argv=argv;
  envp=envp;
}

