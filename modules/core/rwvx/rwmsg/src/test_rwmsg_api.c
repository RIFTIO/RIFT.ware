/* STANDARD_RIFT_IO_COPYRIGHT */
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

