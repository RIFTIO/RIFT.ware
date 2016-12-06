
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

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <msgpack.h>

#include "reaper_client.h"

/*
 * Send a message pack buffer over the specified socket
 *
 * @param fd  - socket to send on
 * @param buf - msgpack_sbuffer to send
 * @return    - 0 on success, -1 otherwise
 */
static int send_msgpack_buffer(int fd, msgpack_sbuffer * buf)
{
  int r = 0;
  ssize_t sent = 0;


  sent = 0;
  r = 0;
  while (true) {
    ssize_t just_sent;

    just_sent = send(fd, buf->data + sent, buf->size - sent, 0);
    if (just_sent < 0) {
      r = -1;
      break;
    }

    sent += just_sent;
    if (sent == buf->size)
      break;
  }

  return r;
}

int reaper_client_connect(const char * path, struct timeval * timeout) {
  int r;
  int fd;
  struct sockaddr_un addr;
  struct timeval stop_at;
  struct timeval current_time;;

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (!fd)
    return errno;

  bzero(&addr, sizeof(struct sockaddr_un));

  addr.sun_family = AF_UNIX;
  r = snprintf(addr.sun_path, UNIX_PATH_MAX, path);
  if (r <= 0) {
    r = ENOMEM;
    goto done;
  }

  if (timeout) {
    r = gettimeofday(&current_time, NULL);
    if (r) {
      r = errno;
      goto done;
    }

    timeradd(&current_time, timeout, &stop_at);
  } else {
    timerclear(&stop_at);
  }

  while (true) {
    if (timerisset(&stop_at)) {
      r = gettimeofday(&current_time, NULL);
      if (r) {
        r = errno;
        goto done;
      }

      if (timercmp(&current_time, &stop_at, >)) {
        r = ETIMEDOUT;
        goto done;
      }
    }

    errno = 0;
    r = connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un) - 1);
    if (r == 0)
      break;
    else if (errno != ECONNREFUSED && errno != ENOENT) {
      r = errno;
      break;
    }

    usleep(100);
  }

done:
  if (r) {
    close(fd);
    errno = r;
    return -1;
  }

  return fd;
}

void reaper_client_disconnect(int reaper_fd) {
  close(reaper_fd);
}

int reaper_client_add_pid(int reaper_fd, pid_t pid) {
  int r;
  msgpack_sbuffer buf;
  msgpack_packer packer;

  msgpack_sbuffer_init(&buf);
  msgpack_packer_init(&packer, &buf, msgpack_sbuffer_write);

  msgpack_pack_map(&packer, 1);
  msgpack_pack_str(&packer, 7);
  msgpack_pack_str_body(&packer, "add_pid", 7);
  msgpack_pack_int(&packer, pid);

  r = send_msgpack_buffer(reaper_fd, &buf);
  msgpack_sbuffer_destroy(&buf);

  return r;
}

int reaper_client_del_pid(int reaper_fd, pid_t pid) {
  int r;
  msgpack_sbuffer buf;
  msgpack_packer packer;

  msgpack_sbuffer_init(&buf);
  msgpack_packer_init(&packer, &buf, msgpack_sbuffer_write);

  msgpack_pack_map(&packer, 1);
  msgpack_pack_str(&packer, 7);
  msgpack_pack_str_body(&packer, "del_pid", 7);
  msgpack_pack_int(&packer, pid);

  r = send_msgpack_buffer(reaper_fd, &buf);
  msgpack_sbuffer_destroy(&buf);

  return r;
}

int reaper_client_add_path(int reaper_fd, const char * path)
{
  int r;
  msgpack_sbuffer buf;
  msgpack_packer packer;

  msgpack_sbuffer_init(&buf);
  msgpack_packer_init(&packer, &buf, msgpack_sbuffer_write);

  msgpack_pack_map(&packer, 1);
  msgpack_pack_str(&packer, 8);
  msgpack_pack_str_body(&packer, "add_path", 8);
  msgpack_pack_str(&packer, strlen(path));
  msgpack_pack_str_body(&packer, path, strlen(path));

  r = send_msgpack_buffer(reaper_fd, &buf);
  msgpack_sbuffer_destroy(&buf);

  return r;
}


