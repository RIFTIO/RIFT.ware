
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "rwlib.h"
#include "rwtypes.h"
#include "rw_sys.h"


rw_status_t rw_instance_uid(uint8_t * uid) {
  rw_status_t ret = RW_STATUS_SUCCESS;
  char * env_def = NULL;

  env_def = getenv("RIFT_INSTANCE_UID");
  if (env_def) {
    long int cast;

    cast = strtol(env_def, NULL, 10);
    if (cast < 0 || cast >= RW_MAXIMUM_INSTANCES) {
      ret = RW_STATUS_OUT_OF_BOUNDS;
      goto done;

    }

    *uid = (uint8_t)cast;
  } else {
    uid_t sys_uid;

    /* Yes, uid not euid.  We don't want this to change if some proceses
     * are launched via sudo or have setuid.
     */
    sys_uid = getuid();
    *uid = (uint8_t)(sys_uid % (RW_MAXIMUM_INSTANCES - RW_RESERVED_INSTANCES));
    RW_STATIC_ASSERT(RW_MAXIMUM_INSTANCES % RW_RESERVED_INSTANCES == 0);
    /* Reserve 0, 16, 32, 48, etc. for autamated unit-testing,  and shift the uid accordingly */
    int i;
    for (i=0; i<RW_RESERVED_INSTANCES; i++) {
      if (*uid < ((i+1)*(RW_MAXIMUM_INSTANCES/RW_RESERVED_INSTANCES)-(i+1))) {
        *uid += (i+1);
        break;
      }
    }
  }

done:
  return ret;
}

rw_status_t rw_unique_port(uint16_t base_port, uint16_t * unique_port)
{
  rw_status_t status;
  uint8_t uid;
  uint32_t port;

  status = rw_instance_uid(&uid);
  if (status != RW_STATUS_SUCCESS)
    return status;

  port = (uint32_t)uid + base_port;

  if (port > 65535)
    return RW_STATUS_FAILURE;

  *unique_port = (uint16_t)port;

  return RW_STATUS_SUCCESS;
}


#define PORT_AVOID_LIST_SIZE (10*1024)
static uint16_t port_avoid_list[PORT_AVOID_LIST_SIZE];
static uint16_t port_avoid_list_count = 0;

gboolean rw_port_in_avoid_list(uint16_t port, uint16_t range)
{
  if (range < 1)
    return FALSE;

  if (port_avoid_list_count == 0) { /* initialize the port_avoid_list[] */
    FILE *in;
    extern FILE *popen();
    char buff[512];

    /* popen creates a pipe so we can read the output
    of the program we are invoking */
    if ((in = popen("netstat -ntapl | cut -d: -f 2 | cut -d\" \" -f 1 | sort -n | uniq", "r"))) {
      /* read the output of netstat, one line at a time */
      while (fgets(buff, sizeof(buff), in) != NULL ) {
        long port;
        char *endptr;
        port = strtol(buff, &endptr, 10);
        if (!endptr || (*endptr == '\n' && port != 0)) {
          port_avoid_list[port_avoid_list_count++] = port;
          RW_ASSERT(port_avoid_list_count < PORT_AVOID_LIST_SIZE);
        }
      }
      /* close the pipe */
      pclose(in);
    }
  }

  // Avoid the list of known port#s
  uint32_t i;
  for (i=0; i<port_avoid_list_count; i++) {
    if ((port_avoid_list[i]>=port) && (port_avoid_list[i]<(port+range))) {
      return TRUE;
    }
  }
  return FALSE;
}

#if 1
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define RW_VAR_RIFT "/var/rift"

rw_status_t rw_setenv(const char *name, const char *value)
{
  rw_status_t ret = RW_STATUS_SUCCESS;
  DIR* dir1 = NULL;
  char command[999];
  const char *var_rift = getenv("RIFT_VAR_ROOT");
  gboolean do_sudo = var_rift == NULL;

  RW_ASSERT(name);
  RW_ASSERT(value);

  if (strchr(name, '/')) {
    ret = RW_STATUS_FAILURE;
    goto ret;
  }

  sprintf(command,
          "%s",
          (var_rift? var_rift: RW_VAR_RIFT));
  dir1 = opendir(command);
  if (!dir1
      && ENOENT != errno) {
    /* opendir() failed to open '/var/rift' for some reason
     * other than 'Not Exist'
     */
    perror("opendir"); ret = RW_STATUS_FAILURE; goto ret;
  } else {
    if (dir1) {
      /* '/var/rift' Directory exists. */
      closedir(dir1);
    }
    sprintf(command,
            "%s mkdir -p %s/env.d",
            (do_sudo? "sudo" : ""),
            (var_rift? var_rift: RW_VAR_RIFT));
    if (system(command)==-1) {
      perror("system mkdir"); ret = RW_STATUS_FAILURE; goto ret;
    }

    sprintf(command,
            "echo -n '%s' | %s tee %s/env.d/%s > /dev/null",
            value,
            (do_sudo? "sudo" : ""),
            (var_rift? var_rift: RW_VAR_RIFT),
            name);
    if (system(command)==-1) {
      perror("system"); ret = RW_STATUS_FAILURE; goto ret;
    }
    ret = RW_STATUS_SUCCESS;
  }
ret:
  return ret;
}

const char *rw_getenv(const char *name)
{
  DIR* dir1 = NULL;
  char command[999];
  static char buff[999];
  FILE *fp;
  const char *var_rift = getenv("RIFT_VAR_ROOT");
  gboolean do_sudo = var_rift == NULL;

  RW_ASSERT(name);

  if (strchr(name, '/')) {
    return NULL;
  }

  sprintf(command, "%s/env.d",
          (var_rift? var_rift: RW_VAR_RIFT));

  dir1 = opendir(command);
  if (dir1) {
    sprintf(command,
            "%s cat %s/env.d/%s",
            (do_sudo? "sudo" : ""),
            (var_rift? var_rift: RW_VAR_RIFT),
            name);
    if ((fp = popen(command, "r"))) {
      if (fgets(buff, sizeof(buff), fp) != NULL) {
        return buff;
      }
    }
  }
  return NULL;
}

void rw_unsetenv(const char *name)
{
  DIR* dir1 = NULL;
  char command[999];
  const char *var_rift = getenv("RW_VAR_RIFT");
  gboolean do_sudo = var_rift == NULL;

  RW_ASSERT(name);

  if (strchr(name, '/')) {
    return;
  }


  sprintf(command, "%s/env.d",
          (var_rift? var_rift: RW_VAR_RIFT));

  dir1 = opendir(command);
  if (dir1) {
    sprintf(command,
            "%s rm %s/env.d/%s",
            (do_sudo? "sudo" : ""),
            (var_rift? var_rift: RW_VAR_RIFT),
            name);
    if (system(command)==-1) {
      perror("system");
    }
  }
  return;
}
#endif
