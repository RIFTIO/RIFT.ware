
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libreaper.h"

static void help() {
  printf("reaperd [ARGUMENTS]\n");
  printf("\n");
  printf("The reaper starts a daemon which binds an AF_UNIX socket to the specified\n");
  printf("path.  Applications can connect to the socket and send msgpack encoded commands.\n");
  printf("Note that this opens up lots of interesting DOS options if the reaper is run as\n");
  printf("another user or as root as anyone can connect without authentication.\n");
  printf("\n");
  printf("COMMANDS:\n");
  printf("    add_pid <pid>     Add a pid to be reaped when the connection is closed:\n");
  printf("    del_pid <pid>     Del a pid from the reaper:\n");
  printf("    add_path <path>   Add a path to be unlinked when the connection is closed:\n");
  printf("\n");
  printf("ARGUMENTS:\n");
  printf("    -h,--help                   This screen\n");
  printf("    -s,--socket_path [PATH]     Path to the socket to bind\n");
  printf("    -k,--kill_all               Kill all client and exit on the first disconnection\n");
}

int main(int argc, char ** argv) {
  struct reaper * reaper;
  char * socket_path = NULL;
  reaper_on_disconnect cb = NULL;

  while (true) {
    int c;
    int index = 0;

    static struct option long_options[] = {
      {"socket_path", required_argument,  0,  's'},
      {"help",        no_argument,        0,  'h'},
      {"kill_all",    no_argument,        0,  'k'},
      {0,             0,                  0,  0}
    };

    c = getopt_long(argc, argv, "hs:k", long_options, &index);

    if (c == -1)
      break;

    switch (c) {
      case 'h':
        help();
        return 0;

      case 's':
        socket_path = strdup(optarg);
        if (!socket_path) {
          err("strdup: %s\n", strerror(errno));
          return 1;
        }
        break;

      case 'k':
        cb = &reaper_exit_on_disconnect;
        break;
    }
  }

  if (!socket_path) {
    err("socket_path (-s) must be specified\n");
    return 1;
  }

  info("Creating reaper at %s\n", socket_path);

  reaper = reaper_alloc(socket_path, cb);
  if (!reaper) {
    err("Failed to create a reaper\n");
    return 1;
  }

  /*
   * If the reaper is left in its original process group, ^C in gdb may
   * suspend the process. As a result the reaper may persist after exiting the
   * GDB.
   */
  setpgid(0, 0);

  reaper_loop(reaper);
  reaper_free(&reaper);

  return 0;
}

