
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <fcntl.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#include <rwvcs_rwzk.h>

#include "rwmain.h"

/*
 * Callback when the read side of an open pipe to a launched process had
 * something in its buffer.  This is a trick to monitor spawned processes to
 * see when they exit.  Before forking we create a pipe and leave that open on
 * both sides.  Therefore, when the spawned process exits, the pipe will be
 * closed and the CFSocketRef will raise a read event.
 *
 * This could be used to enact any number of policies dealing with processes
 * when they exit, however for now we'll simple abort.
 */
static void rwproc_on_io_ready(
    rwsched_CFSocketRef sock,
    CFSocketCallBackType type,
    CFDataRef address,
    const void *data,
    void *info);


struct rwmain_proc * rwmain_proc_alloc(
    struct rwmain_gi * rwmain,
    const char * instance_name,
    bool is_rwproc,
    pid_t pid,
    int pipe_fd)
{
  struct rwmain_proc * rp;
  int r;
  CFSocketContext context;
  rwsched_CFSocketRef cfsocket;


  rp = (struct rwmain_proc *)malloc(sizeof(struct rwmain_proc));
  if (!rp) {
    RW_CRASH();
    goto err;
  }
  bzero(rp, sizeof(struct rwmain_proc));

  rp->rwmain = rwmain;
  rp->pid = pid;
  rp->instance_name = strdup(instance_name);
  rp->is_rwproc = is_rwproc;
  if (!rp->instance_name) {
    RW_CRASH();
    goto err;
  }

  bzero(&context, sizeof(context));
  context.info = (void *)rp;

  /*
   * We're using a CFSocket here because CFFileDescriptor hasn't been
   * ported to Linux yet.  This is ok so long as we make sure not to use
   * any socket specific calls on the file descriptor.  This means using
   * kCFSocketDataCallBack instead of kCFSocketReadCallback as the latter
   * also handles accept() calls.
   *
   * Also, the cfsocket gets updated to hold a pointer to the cfsource, so
   * don't release it.  Ironically, we could release the cfsource if we
   * didn't need it later.
   */
  cfsocket = rwsched_tasklet_CFSocketCreateWithNative(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorSystemDefault,
      pipe_fd,
      kCFSocketDataCallBack,
      rwproc_on_io_ready,
      &context);
  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);

  rp->cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorSystemDefault,
      cfsocket,
      0);

  rwsched_tasklet_CFRunLoopAddSource(
      rwmain->rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
      rp->cfsource,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);

  /* Make sure the pipe hasn't already closed */
  r = fcntl(pipe_fd, F_SETFL, O_NONBLOCK);
  if (r) {
    RW_CRASH();
    goto err;
  }

  r = read(pipe_fd, NULL, 0);
  if (r != 0 && errno == EBADF) {
    RW_CRASH();
    goto err;
  }

  return rp;

err:
  if (rp->cfsource) {
    rwsched_tasklet_CFRunLoopRemoveSource(
        rp->rwmain->rwvx->rwsched_tasklet,
        rwsched_tasklet_CFRunLoopGetCurrent(rp->rwmain->rwvx->rwsched_tasklet),
        rp->cfsource,
        rp->rwmain->rwvx->rwsched->main_cfrunloop_mode);
    CFRelease(rp->cfsource);
  }

  if (rp->instance_name)
    free(rp->instance_name);

  if (rp)
    free(rp);

  return NULL;
}

void rwmain_proc_free(struct rwmain_proc * rp)
{
  if (rp->cfsource) {
    rwsched_tasklet_CFRunLoopRemoveSource(
        rp->rwmain->rwvx->rwsched_tasklet,
        rwsched_tasklet_CFRunLoopGetCurrent(rp->rwmain->rwvx->rwsched_tasklet),
        rp->cfsource,
        rp->rwmain->rwvx->rwsched->main_cfrunloop_mode);
    CFRelease(rp->cfsource);
    rp->cfsource = NULL;
  }

  if (rp->pid) {
    int r;
    pid_t wait_r;
    int wait_status;

    r = kill(rp->pid, 0);
    if (r != -1 || errno != ESRCH) {
      rwmain_trace_info(
          rp->rwmain,
          "Sending TERM to %s (%d)",
          rp->instance_name,
          rp->pid);
      kill(rp->pid, SIGTERM);

      for (size_t i = 0; i < 5000; ++i) {
        wait_r = waitpid(rp->pid, &wait_status, WNOHANG);
        if (wait_r == rp->pid)
          break;
        usleep(1000);
      }

      if (wait_r != rp->pid) {
        rwmain_trace_info(
            rp->rwmain,
            "Sending KILL to %s (%d)",
            rp->instance_name,
            rp->pid);
        kill(rp->pid, SIGKILL);
        waitpid(rp->pid, &wait_status, 0);
      }
    } else {
      waitpid(rp->pid, &wait_status, 0);
    }
  }

  if (rp->instance_name)
    free(rp->instance_name);

  free(rp);
}

static void rwproc_on_io_ready(
    rwsched_CFSocketRef sock,
    CFSocketCallBackType type,
    CFDataRef address,
    const void * data,
    void * info)
{
  rw_status_t status;
  struct rwmain_proc * rp;
  struct rwmain_proc * junk;

  UNUSED(type);
  UNUSED(address);
  UNUSED(data);

  rp = (struct rwmain_proc *)info;

  rwmain_trace_info(
      rp->rwmain,
      "Process %s closed the status pipe, pid %u",
      rp->instance_name,
      rp->pid);

  status = RW_SKLIST_REMOVE_BY_KEY(&(rp->rwmain->procs), &rp->instance_name, &junk);
  if (status != RW_STATUS_SUCCESS)
    rwmain_trace_crit(rp->rwmain, "Failed to remove %s from rwmain procs", rp->instance_name);

  if (rp->is_rwproc) {
    restart_process(rp->rwmain, rp->instance_name);
  } else {
    status = rwvcs_rwzk_update_state(
        rp->rwmain->rwvx->rwvcs,
        rp->instance_name,
        RW_BASE_STATE_TYPE_CRASHED);
    if (status != RW_STATUS_SUCCESS && status != RW_STATUS_NOTFOUND) {
      rwmain_trace_crit(
          rp->rwmain,
          "Failed to update %s state to CRASHED",
          rp->instance_name);
    }
    else {
      rwmain_trace_crit(
          rp->rwmain,
          "%s CRASHED",
          rp->instance_name);
    }
  }

  rwmain_proc_free(rp);
}
