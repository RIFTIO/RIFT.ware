/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_proc.c
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 04/26/2013
 * @brief Riftware utilities for process handling
 * 
 * @details 
 *
 */

#include "rwlib.h"
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

rw_status_t rw_spawn_process(const char *filename, 
                             char *const argv[],
                             int set_pgid,
                             pid_t *child_pid)
{
  pid_t pid;

  pid = fork();
  if (pid < 0) {
    /* fork failed emit error and return */
    perror(NULL);
    return RW_STATUS_SUCCESS;
  } else if (pid == 0) {
    /* child process */

    fprintf(stdout, "filename: %s\n", filename);
    int i = 0;
    while (argv[i] != NULL) {
      fprintf(stdout, "argv[%d]: %s\n", i, argv[i]);
      i++;
    }

    execvp(filename, argv);
    /* should never reach here, unless execvp failed */
    perror(NULL);
    RW_ASSERT_NOT_REACHED();
  } else {
    /* This is the parent process */
    if (set_pgid) {
      /* set the process group id of the child to parent's process group id */
      if (setpgid(pid, getpgrp())) {
        fprintf(stdout, "Failed to set %s (%d) process group id\n", 
                filename, pid);
      }
    }

    if (child_pid) {
      *child_pid = pid;
    }
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_terminate_process(pid_t pid)
{
  rw_status_t status;

  if (kill(pid, SIGKILL) < 0) {
    perror(NULL);
    status = RW_STATUS_FAILURE;
  } else {
    status = RW_STATUS_SUCCESS;
  }

  return status;
}

