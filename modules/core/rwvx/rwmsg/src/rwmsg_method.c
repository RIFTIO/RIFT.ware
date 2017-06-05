/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwmsg_method.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG method object
 */

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/types.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <ctype.h>

#include <rwlib.h>
#include <rw_rand.h>

#include "rwmsg_int.h"

rwmsg_method_t *rwmsg_method_create(rwmsg_endpoint_t *ep,
				    const char *path,
				    rwmsg_signature_t *sig,
				    rwmsg_closure_t *requestcb) {
  RW_ASSERT(path);
  RW_ASSERT(sig);
  
  rwmsg_method_t *meth = NULL;

  uint64_t pathhash = 0;
  int32_t pathidx = 0;

  if (requestcb) {
    pathidx = rwmsg_endpoint_path_add(ep, path, &pathhash);
  } else {
    pathhash = rwmsg_endpoint_path_hash(ep, path);
  }
  RW_ASSERT(pathhash);

  if (pathidx || !requestcb/*null cb in broker, ignore other tasklet's entries for small model broker */) {
    meth = (rwmsg_method_t*)RW_MALLOC0_TYPE(sizeof(*meth), rwmsg_method_t);
    meth->pathidx = pathidx;
    meth->pathhash = pathhash;
    strncpy(meth->path, path, sizeof(meth->path));
    meth->path[sizeof(meth->path)-1] = '\0';
    ck_pr_inc_32(&sig->refct);
    meth->sig = sig;
    if (requestcb) {
      meth->cb = *requestcb;
    }
    meth->accept_fd = -1;
    
    ck_pr_inc_32(&ep->stat.objects.methods);
    meth->refct=1;
  }

  return meth;
}

void rwmsg_method_destroy(rwmsg_endpoint_t *ep,
			  rwmsg_method_t *meth) {
  if (!meth->refct) {
    if (meth->pathidx) {
      rwmsg_bool_t prval = rwmsg_endpoint_path_del(ep, meth->pathidx);
      RW_ASSERT(prval);
    }

    RW_ASSERT(meth->sig);
    RW_ASSERT(meth->sig->refct > 0);
    rwmsg_signature_release(ep, meth->sig);

    RW_FREE_TYPE(meth, rwmsg_method_t);
    ck_pr_dec_32(&ep->stat.objects.methods);
  }
}

void rwmsg_method_release(rwmsg_endpoint_t *ep,
			  rwmsg_method_t *meth) {
  bool zero=0;
  ck_pr_dec_32_zero(&meth->refct, &zero);
  if (zero) {
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_METHOD, meth, NULL, NULL);
  }
}

