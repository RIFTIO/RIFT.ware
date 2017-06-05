
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>

#include "libreaper.h"

static void void_handler(int sig) {
  (void)sig;
}

/*
 * Return a string that is properly terminated as msgpack treats strings as
 * binary data.  Note that the caller will be required to free the returned
 * string
 *
 * @param msg - A message pack object containing a string.
 * @return    - A newly allocated string containing a null-terminated copy
 *              of the string contained in the specified message.  It is up
 *              to the caller to free this string.  If the message did not
 *              contain a string, NULL will be returned.
 */
static inline char * get_string(msgpack_object * msg)
{
  if (msg->type != MSGPACK_OBJECT_STR)
    return NULL;

  char * p = (char *)malloc((msg->via.str.size + 1) * sizeof(char));
  memcpy(p, msg->via.str.ptr, msg->via.str.size);
  p[msg->via.str.size] = '\0';
  return p;
}

/*
 * Lookup a client by socket.
 *
 * @param reaper  - reaper instance
 * @param socket  - client socket
 * @return        - client instance if found, otherwise NULL
 */
static struct client * lookup_client(struct reaper * reaper, int socket) {
  struct client * client;

  SLIST_FOREACH(client, &reaper->clients, slist) {
    if (client->socket == socket)
      return client;
  }

  return NULL;
}

/*
 * Handle a msgpack message.
 *
 * @param reaper  - reaper instance
 * @param client  - client instance which sent the message
 * @param obj     - msgpack msg
 * @return        - 0 on success, negative otherwise
 */
static int handle_message(struct reaper * reaper, struct client * client, msgpack_object * obj)
{
  msgpack_object * key;
  msgpack_object * val;

  if (obj->type != MSGPACK_OBJECT_MAP) {
    err("message is not a map\n");
    return -1;
  }

  if (obj->via.map.size != 1) {
    err("invalid map size: %u != 1\n", obj->via.map.size);
    return -1;
  }

  key = &(obj->via.map.ptr[0].key);
  val = &(obj->via.map.ptr[0].val);

  if (key->type != MSGPACK_OBJECT_STR) {
    err("invalid map key type: %d\n", key->type);
    return -1;
  }

  if (key->via.str.size == 7 && !strncmp(key->via.str.ptr, "add_pid", 7)) {
    int r;

    if (val->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
      err("invalid value type for add_pid: %d\n", val->type);
      return -1;
    }

    info("adding pid %lu to client %d\n", val->via.u64, client->socket);
    r = reaper_add_client_pid(reaper, client, val->via.u64);
    (void)r;
    // TODO:  how to handle error?
  } else if (key->via.str.size == 8 && !strncmp(key->via.str.ptr, "add_path", 8)) {
    int r;
    char * path;

    if (val->type != MSGPACK_OBJECT_STR) {
      err("invalid value type for add_path: %d\n", val->type);
      return -1;
    }

    path = get_string(val);
    if (!path) {
      err("Failed to get path from msgpack object\n");
      return -1;
    }

    info("adding path %s to client %d\n", path, client->socket);
    r = reaper_add_client_path(reaper, client, path);
    free(path);
    (void)r;
    // TODO:  how to handle error?
  } else if (key->via.str.size == 7 && !strncmp(key->via.str.ptr, "del_pid", 7)) {
    int r;

    if (val->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
      err("invalid value type for del_pid: %d\n", val->type);
      return -1;
    }

    info("del pid %lu from client %d\n", val->via.u64, client->socket);
    r = reaper_del_client_pid(reaper, client, val->via.u64);
    (void)r;
    // TODO:  how to handle error?
  } else {
    char * cmd;

    cmd = get_string(key);
    if (!cmd) {
      err("unknown command (and malloc to get cmd name failed)\n");
    } else {
      err("unknown command: %s\n", cmd);
      free(cmd);
    }
    return -1;
  }

  return 0;
}

struct client * client_alloc(int socket) {
  struct client * client;

  client = (struct client *)malloc(sizeof(struct client));
  if (!client) {
    err("malloc: %s\n", strerror(errno));
    return NULL;
  }

  client->mpac = msgpack_unpacker_new(1024);
  if (!client->mpac) {
    err("msgpack_unpacker_new(1024) failed\n");
    free(client);
    return NULL;
  }

  client->max_pids = 64;
  client->pids = (pid_t *)malloc(sizeof(pid_t) * client->max_pids);
  if (!client->pids) {
    err("malloc: %s\n", strerror(errno));
    free(client);
    return NULL;
  }
  bzero(client->pids, sizeof(pid_t) * client->max_pids);

  client->max_paths = 64;
  client->paths = (char **)malloc(sizeof(char *) * client->max_paths);
  if (!client->paths) {
    err("malloc: %s\n", strerror(errno));
    free(client->pids);
    free(client);
    return NULL;
  }
  bzero(client->paths, sizeof(char *) * client->max_paths);

  client->socket = socket;

  return client;
}

void client_free(struct client ** client) {
  free((*client)->pids);

  for (size_t i = 0; i < (*client)->max_paths; ++i) {
    if (!(*client)->paths[i])
      break;
    free((*client)->paths[i]);
  }
  free((*client)->paths);

  msgpack_unpacker_free((*client)->mpac);
  free(*client);

  *client = NULL;
}

int reaper_add_client(struct reaper * reaper, int socket) {
  struct client * client;

  client = client_alloc(socket);
  if (!client)
    return -1;

  SLIST_INSERT_HEAD(&reaper->clients, client, slist);

  return 0;
}

int reaper_del_client_pid(struct reaper * reaper, struct client * client, pid_t pid) {

  for (size_t i = 0; i < client->max_pids; ++i) {
    if (client->pids[i] == pid) {
      client->pids[i] = 0;
      return 0;
    }
  }

  return -1;
}

int reaper_add_client_pid(struct reaper * reaper, struct client * client, pid_t pid) {
  bool inserted = false;

  for (size_t i = 0; i < client->max_pids; ++i) {
    if (!client->pids[i]) {
      client->pids[i] = pid;
      inserted = true;
      break;
    }
  }

  if (!inserted) {
    pid_t * new_pids;

    new_pids = (pid_t *)realloc(client->pids, sizeof(pid_t) * 2 * client->max_pids);
    if (!new_pids) {
      err("failed to increase client pid list: %s\n", strerror(errno));
      return -1;
    }

    client->pids = new_pids;
  }

  return 0;
}

int reaper_add_client_path(struct reaper * reaper, struct client * client, const char * path) {
  bool inserted = false;

  for (size_t i = 0; i < client->max_paths; ++i) {
    if (!client->paths[i]) {
      client->paths[i] = strdup(path);
      if (!client->paths[i])
        return -1;

      inserted = true;
      break;
    }
  }

  if (!inserted) {
    char ** new_paths;

    new_paths = (char **)realloc(client->paths, sizeof(char *) * 2 * client->max_paths);
    if (!new_paths) {
      err("failed to increase client path list: %s\n", strerror(errno));
      return -1;
    }

    client->paths = new_paths;
  }

  return 0;
}

static int reaper_check_filename_pid(const char *str, const char *pid_str) {
  int retval = 0;
  const char *pidptr = strstr(str, pid_str);
  if (pidptr) {
    /* Either end of string or non-digit surrounding the pid */
    int before = (pidptr == str || (pidptr > str && !isdigit(pidptr[-1])));
    int after = ((pidptr + strlen(pid_str) == str + strlen(str))
		 || !isdigit(pidptr[strlen(pid_str)]));
      
    retval = (before && after);
  }
  return retval;
}

#define REAPER_SLEEP_MS(x) {						\
  struct timespec ts = { .tv_sec = (x)/1000, .tv_nsec = ((x) % 1000) * 1000000 }; \
  int rv = 0;								\
  do {									\
    rv = nanosleep(&ts, &ts);						\
  } while (rv == -1 && errno == EINTR);					\
}


static void reaper_wait_for_core(pid_t pid) {
  DIR *dir = NULL;
  char cwdpath[1024];
  char corepath[2048];

  assert(sizeof(pid) == sizeof(int32_t));
  sprintf(cwdpath, "/proc/%"PRIi32"/cwd", (int32_t)pid);
  ssize_t s = readlink(cwdpath, corepath, 2048);
  if (s > 0 && s < 2048) {
    corepath[s] = '\0';
    /* corepath is the process's cwd. */
  } else {
    if (!getcwd(corepath, 2048)) {
      /* Meh, path too long */
      return;
    }
  }
  
  /* scan cwd, of each process per proc, or our cwd if nothing better */
  char pid_str[16];
  int len = snprintf(pid_str, 16, "%"PRIi32, (int32_t)pid);
  if (len > 0 && len < 16) {
    pid_str[15] = '\0';
    dir = opendir(corepath);
    if (dir) {
      info("reaper checking for pid %"PRIi32" cores in '%s'\n", (int32_t)pid, corepath);
      struct dirent *dent;
      for (dent = readdir(dir);
	   dent;
	   dent = readdir(dir)) {
	if (strstr(dent->d_name, "core")
	    && reaper_check_filename_pid(dent->d_name, pid_str)) {

	  struct stat statbuf = { };
	  int r = stat(dent->d_name, &statbuf);
	  if (r == 0 && statbuf.st_mtime >= time(NULL) - 15) {
	    int last_update = 0;
	    err("waiting on core file '%s'\n", dent->d_name);
	    int ii;
	    for (ii = 0; ii < 240; ii += 5) {
	      struct stat statbuf2 = { };
	      int r = stat(dent->d_name, &statbuf2);
	      if (r == 0) {
		if (statbuf.st_mtime == statbuf2.st_mtime
		    && statbuf.st_size == statbuf2.st_size) {
		  if (ii - last_update > 20) {
		    /* No change in 15s */
		    goto ret;
		  }
		} else {
		  last_update = ii;
		  memcpy(&statbuf, &statbuf2, sizeof(statbuf));
		}
	      } else {
		/* Vanished? Go to next file */
		break;
	      }
	      REAPER_SLEEP_MS(5000);
	      info(" +5s core file '%s'\n", dent->d_name);
	    }
	  }
	}
      }
    }
  }
  
 ret:
  if (dir) {
    closedir(dir);
  }
  return;
}  

int reaper_kill_client_pids(struct reaper * reaper, struct client * client) {
  int r;
  size_t cleared = 0;
  size_t n_pids;


  for (n_pids = 0; client->pids[n_pids]; ++n_pids) {

    /* Check for and wait on core file */
    reaper_wait_for_core(client->pids[n_pids]);
    
    info("Sending SIGTERM to %u\n", client->pids[n_pids]);
    r = kill(-1 * client->pids[n_pids], SIGTERM);
    if (r && errno != ESRCH)
      err("kill(SIGTERM, %"PRIi32"): %s\n", -1 * (int32_t)client->pids[n_pids], strerror(errno));
  }

  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < n_pids; ++j) {
      if (client->pids[j]) {
        errno = 0;
        r = kill(client->pids[j], 0);
        if (r != -1 || errno != ESRCH)
          break;

        info("%"PRIi32" exited on SIGTERM\n", (int32_t)client->pids[j]);
        cleared++;
        client->pids[j] = 0;
      }
    }

    if (cleared == n_pids)
      break;

    usleep(5000);
  }

  for (size_t i = 0; i < n_pids; ++i) {
    if (client->pids[i]) {
      info("Sending SIGKILL TO %"PRIi32"\n", (int32_t)client->pids[i]);
      r = kill(-1 * (pid_t)client->pids[i], SIGKILL);
      if (r)
        err("kill(SIGTERM, %"PRIi32"): %s\n", -1 * (int32_t)client->pids[i], strerror(errno));
    }
  }

  return 0;
}

int reaper_unlink_client_paths(struct reaper * reaper, struct client * client) {
  int r = 0;

  for (size_t i = 0; i < client->max_paths; ++i) {
    int unlink_r;

    if (!client->paths[i])
      break;

    info("Unlinking %s for client %d\n", client->paths[i], client->socket);

    unlink_r = unlink(client->paths[i]);
    if (unlink_r && errno != ENOENT) {
      err("unlink(%s): %s\n", client->paths[i], strerror(errno));
      r = -1;
    }
  }

  return r;
}

struct reaper * reaper_alloc(const char * ipc_path, reaper_on_disconnect cb) {
  int r;
  struct reaper * reaper;
  struct sockaddr_un addr;
  mode_t old_mask;

  assert(sizeof(pid_t) == sizeof(int32_t));

  reaper = (struct reaper *)malloc(sizeof(struct reaper));
  if (!reaper) {
    err("malloc: %s\n", strerror(errno));
    return NULL;
  }

  reaper->stop = false;
  SLIST_INIT(&reaper->clients);
  if (cb)
    reaper->on_disconnect = cb;
  else
    reaper->on_disconnect = &reaper_default_on_disconnect;

  reaper->socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (reaper->socket < 0) {
    err("socket: %s\n", strerror(errno));
    goto error;
  }

  /* Create the socket node
   * ----------------------
   * Note `SO_REUSEADDR` does not work with `AF_UNIX` sockets,
   * so we will have to unlink the socket node if it already exists,
   * before we bind. For safety, we won't unlink an already existing node
   * which is not a socket node. 
   */

  struct stat st;
  r = stat (ipc_path, &st);
  if (r == 0) {
    /* A file already exists. Check if this file is a socket node.
     *   * If yes: unlink it.
     *   * If no: treat it as an error condition.
     */
    if ((st.st_mode & S_IFMT) == S_IFSOCK) {
      err ("The path '%s' already exists and is a socket node; unlinking & reusing\n", ipc_path);
      r = unlink (ipc_path);
      if (r != 0) {
        err ("Error unlinking the socket node: %s", strerror(errno));
        goto close_socket;
      }
    }
    else {
      /* We won't unlink to create a socket in place of who-know-what.
       * Note: don't use `perror` here, as `r == 0` (this is an
       * error we've defined, not an error returned by a system-call).
       */
      err ("The path already exists and is not a socket node.\n");
      goto close_socket;
    }
  }
  else {
    if (errno == ENOENT) {
      /* No file of the same path: do nothing. */
    }
    else {
      err ("Error stating the socket node path: %s", strerror(errno));
      goto close_socket;
    }
  }

  bzero(&addr, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  r = snprintf(addr.sun_path, UNIX_PATH_MAX, "%s", ipc_path);
  if (r < 0 || r >= UNIX_PATH_MAX) {
    err("snprintf: %s\n", strerror(errno));
    goto close_socket;
  }

  r = snprintf(reaper->ipc_path, UNIX_PATH_MAX, "%s", ipc_path);
  if (r < 0 || r >= UNIX_PATH_MAX) {
    err("snprintf: %s\n", strerror(errno));
    goto close_socket;
  }


  old_mask = umask(0);
  r = bind(reaper->socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_un) -1);
  if (r) {
    err("bind: %s\n", strerror(errno));
    goto close_socket;
  }
  umask(old_mask);

  r = listen(reaper->socket, 10);
  if (r) {
    err("listen: %s\n", strerror(errno));
    goto close_socket;
  }

  return reaper;

close_socket:
  close(reaper->socket);

error:
  free(reaper);

  unlink(ipc_path);

  return NULL;
}

void reaper_free(struct reaper ** reaper) {
  struct client * cp;
  struct client * safe;

  SLIST_FOREACH_SAFE(cp, &(*reaper)->clients, slist, safe) {
    if (cp)
      client_free(&cp);
  }

  reaper_shutdown(*reaper);

  *reaper = NULL;
}

void reaper_shutdown(struct reaper * reaper) {
  if (reaper->socket > 0) {
    close(reaper->socket);
    reaper->socket = -1;
  }

  if (reaper->ipc_path[0] != '\0') {
    unlink(reaper->ipc_path);
    reaper->ipc_path[0] = '\0';
  }
}

void reaper_loop(struct reaper * reaper) {
  int epoll;
  struct epoll_event ev;
  int r;
  struct sigaction sa;
  sigset_t emptyset;
  int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGPIPE, 0};


  r = sigemptyset(&emptyset);
  if (r) {
    err("sigemptyset: %s\n", strerror(errno));
    return;
  }

  for (int i = 0; signals[i]; ++i) {
    r = sigaddset(&sa.sa_mask, signals[i]);
    if (r) {
      err("sigaddset: %s\n", strerror(errno));
      return;
    }
  }

  epoll = epoll_create(1);
  if (epoll < 0) {
    err("epoll_create: %s\n", strerror(errno));
    return;
  }

  ev.events = EPOLLIN;
  ev.data.fd = reaper->socket;

  r = epoll_ctl(epoll, EPOLL_CTL_ADD, reaper->socket, &ev);
  if (r) {
    err("epoll_ctl: %s\n", strerror(errno));
    goto close_epoll;
  }

  sa.sa_handler = void_handler;
  sa.sa_flags = 0;
  for (int i = 0; signals[i]; ++i) {
    r = sigaction(signals[i], &sa, NULL);
    if (r) {
      err("sigaction: %s\n", strerror(errno));
      goto close_epoll;
    }
  }

  while (!reaper->stop) {
    struct epoll_event event;
    int fds;

    fds = epoll_pwait(epoll, &event, 1, -1, &emptyset);

    if (fds == -1) {
      if (errno == EINTR) {
        info("Caught signal, shutting down\n");

        reaper->on_disconnect(reaper, 0);
        break;
      }

      err("epoll_wait: %s\n", strerror(errno));
      continue;
    }

    if (event.data.fd == reaper->socket) {
      int new_sock;
      int flags;

      new_sock = accept(reaper->socket, NULL, NULL);
      info("got new connection\n");

      flags = fcntl(new_sock, F_GETFL);
      r = fcntl(new_sock, F_SETFL, flags|O_NONBLOCK);
      if (r) {
        err("fcntl(flags|O_NONBLOCK): %s\n", strerror(errno));
        close(new_sock);
        continue;
      }

      ev.events = EPOLLIN;
      ev.data.fd = new_sock;

      r = epoll_ctl(epoll, EPOLL_CTL_ADD, new_sock, &ev);
      if (r) {
        err("epoll_ctl(accept): %s\n", strerror(errno));
        close(new_sock);
        continue;
      }

      r = reaper_add_client(reaper, new_sock);
      if (r) {
        close(new_sock);
      }

      continue;
    }

    if (event.events & EPOLLIN) {
      struct client * client;

      client = lookup_client(reaper, event.data.fd);
      if (!client) {
        err("lookup_client(): data on socket %d but no associated client\n", event.data.fd);

        r = epoll_ctl(epoll, EPOLL_CTL_DEL, event.data.fd, NULL);
        if (r)
          err("epoll_ctl(del on close): %s\n", strerror(errno));

        close(event.data.fd);
        continue;
      }

      /* Unpack and handle as many messages as possible for this connection.
       * On error, r is set to non-zero and the connection will be removed from
       * the poller and closed outside of the loop.
       */
      while (true) {
        ssize_t read;
        size_t read_max;
        msgpack_unpack_return ret;
        msgpack_unpacked unpacked;

        r = 0;
        read_max = msgpack_unpacker_buffer_capacity(client->mpac);
        if (!read_max) {
          msgpack_unpacker_expand_buffer(client->mpac, client->mpac->initial_buffer_size * 2);
          continue;
        }

        read = recv(event.data.fd, msgpack_unpacker_buffer(client->mpac), read_max, 0);
        if (read < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;

          err("read: %s\n", strerror(errno));
          r = -1;
          break;
        } else if (read == 0) {
          break;
        }

        msgpack_unpacker_buffer_consumed(client->mpac, read);
        if (read == read_max) {
          msgpack_unpacker_expand_buffer(client->mpac, client->mpac->initial_buffer_size * 2);
          continue;
        }

        msgpack_unpacked_init(&unpacked);

        ret = msgpack_unpacker_next(client->mpac, &unpacked);
        while (ret == MSGPACK_UNPACK_SUCCESS) {
          r = handle_message(reaper, client, &unpacked.data);
          if (r)
            break;
          ret = msgpack_unpacker_next(client->mpac, &unpacked);
        }

        msgpack_unpacked_destroy(&unpacked);
      }

      if (r) {
        reaper->on_disconnect(reaper, client);

        r = epoll_ctl(epoll, EPOLL_CTL_DEL, event.data.fd, NULL);
        if (r)
          err("epoll_ctl(del on read err): %s\n", strerror(errno));
        close(event.data.fd);
      }
    }

    if (event.events & EPOLLERR || event.events & EPOLLHUP) {
      struct client * client;

      info("Connection hung up\n");

      client = lookup_client(reaper, event.data.fd);
      if (!client)
        err("lookup_client(): socket %d closed but no associated client\n", event.data.fd);
      else
        reaper->on_disconnect(reaper, client);

      r = epoll_ctl(epoll, EPOLL_CTL_DEL, event.data.fd, NULL);
      if (r)
        err("epoll_ctl(del on close): %s\n", strerror(errno));

      close(event.data.fd);
    }
  }

close_epoll:
    close(epoll);
}

void reaper_default_on_disconnect(struct reaper * reaper, struct client * client) {
  int r;

  r = reaper_kill_client_pids(reaper, client);
  if (r)
    err("Failed to kill client %d pids on disconnect\n", client->socket);

  r = reaper_unlink_client_paths(reaper, client);
  if (r)
    err("Failed to unlink client %d paths on disconnect\n", client->socket);
}

void reaper_exit_on_disconnect(struct reaper * reaper, struct client * client) {
  int r;
  struct client * cp;

  (void)client;

  reaper_shutdown(reaper);
  reaper->stop = true;

  SLIST_FOREACH(cp, &reaper->clients, slist) {
    r = reaper_kill_client_pids(reaper, cp);
    if (r)
      err("Failed to kill client %d pids on disconnect\n", cp->socket);

    r = reaper_unlink_client_paths(reaper, cp);
    if (r)
      err("Failed to unlink client %d paths on disconnect\n", cp->socket);
  }

  info("Exiting after killing all client pids\n");
}

