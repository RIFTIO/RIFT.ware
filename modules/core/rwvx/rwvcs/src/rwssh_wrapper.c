
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

/**
 * This is a wrapper for remote ssh commands.
 * Normally when SSH connection is closed, the sshd forked process
 * to handle the command will go away, but the actual command still
 * continues to run.
 *
 * For example:
 *  Run - ssh foo sleep 500
 *  Enter ^c
 *  On foo "sleep 500" still executes.
 *
 * This wrapper ensures that when the SSH connection is closed,
 * the remote program will get SIGTERM.
 */
int main (int argc, char** argv)
{
 char *r;
 int i;

  r = strdup(argv[0]);
  for (i=0; i<3; i++)
	  r = dirname(r);
  printf("%s\n", r);
  chdir(r);

  /* treat any x=y args as env vars */
  i=1;
  while ((r = strchr(argv[i],'=')) != NULL) {
	  *r=0;
	  r++;
	  if (setenv(argv[i], r, 1)) {
		  fprintf(stderr, "error setting env var %s\n", argv[i] );
		  return 127;
/*	  } else {
		  printf("set \"%s\" to \"%s\"\n", argv[i], r);
*/
	  }
	  i++;
  }

  prctl(PR_SET_PDEATHSIG, SIGTERM);
  //printf("exec %s\n", argv[i] );
  execvp(argv[i], argv + i);
  return 127;
}
