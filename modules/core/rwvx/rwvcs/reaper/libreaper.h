
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

#ifndef __LIBREAPER_H__
#define __LIBREAPER_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>

#include <msgpack.h>

__BEGIN_DECLS

#define err(fmt, ...)   fprintf(stderr, "reaperd-%d ERROR[%s():%d]: " fmt, getpid(), __func__, __LINE__, ##__VA_ARGS__)
#define info(fmt, ...)  syslog(LOG_USER | LOG_DEBUG, "reaperd-%d INFO[%s():%d]: " fmt, getpid(), __func__, __LINE__, ##__VA_ARGS__)

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

struct client {
  SLIST_ENTRY(client) slist;

  uint16_t * pids;
  size_t max_pids;

  char ** paths;
  size_t max_paths;

  int socket;
  msgpack_unpacker * mpac;
};

struct reaper;
typedef void (*reaper_on_disconnect)(struct reaper *, struct client *);

struct reaper {
  bool stop;
  int socket;
  char ipc_path[UNIX_PATH_MAX];
  reaper_on_disconnect on_disconnect;
  SLIST_HEAD(_client_list, client) clients;
};

/**
 * Create a reaper client connected via the specified socket.
 *
 * @param socket  - client's file descriptor
 * @return        - a new client structure or NULL on error.
 */
struct client * client_alloc(int socket);

/**
 * Free the given client and set the pointer to NULL.
 *
 * @param client  - address of a client pointer.
 */
void client_free(struct client ** client);

/**
 * Add a new client to the reaper at the given socket.
 *
 * @param reaper  - reaper instance
 * @param socket  - file descriptor of the new client
 * @return        - 0 on success.
 */
int reaper_add_client(struct reaper * reaper, int socket);

/**
 * Add a new pid(process) to the specified client.
 *
 * @param reaper  - reaper instance
 * @param client  - client instance
 * @param pid     - pid to add
 * @return        - 0 on success.
 */
int reaper_add_client_pid(struct reaper * reaper, struct client * client, uint16_t pid);

/**
 * Del a pid(process) from the specified client.
 *
 * @param reaper  - reaper instance
 * @param client  - client instance
 * @param pid     - pid to del
 * @return        - 0 on success.
 */
int reaper_del_client_pid(struct reaper * reaper, struct client * client, uint16_t pid);

/**
 * Add a new path to be unlinked to the specified client.
 *
 * @param reaper  - reaper instance
 * @param client  - client instance
 * @param path    - path to add
 * @return        - 0 on success.
 */
int reaper_add_client_path(struct reaper * reaper, struct client * client, const char * path);

/**
 * Kill the processes associated with a client.  This will first
 * send a SIGTERM to each process and then loop for up to 5
 * seconds while the processes still exist.  If they are all
 * gone, the function exits.  If not, a SIGKILL is sent to the
 * remainders.
 *
 * @param reaper  - reaper instance
 * @param client  - client instance
 * @return        - 0 on success, -1 otherwise
 */
int reaper_kill_client_pids(struct reaper * reaper, struct client * client);

/**
 * Unlink all paths associated with a client.  If the path no longer exists
 * (ENOENT) the error will be ignored, otherwise it signals failure (but
 * unlinking will continue for all other paths).
 *
 * @param reaper  - reaper instance
 * @param client  - client instance
 * @return        - 0 on success, -1 otherwise
 */
int reaper_unlink_client_paths(struct reaper * reaper, struct client * client);

/**
 * Create a new reaper.
 *
 * @param ipc_path  - path for the reaper's UNIX socket
 * @param cb        - handler function for clients when they
 *                    disconnect.
 * @return          - new reaper on success, NULL otherwise.
 */
struct reaper * reaper_alloc(const char * ipc_path, reaper_on_disconnect cb);

/*
 * Free the reaper at the specified address.  This will also free
 * any remaining clients of the reaper but will not call the
 * disconnect function on them.
 *
 * @param reaper  - pointer to a reaper instance
 */
void reaper_free(struct reaper ** reaper);

/*
 * Close and unlink Shutdown the reaper's control socket.  This will stop any
 * further clients from being added.
 *
 * @param reaper  - reaper instance
 */
void reaper_shutdown(struct reaper * reaper);

/*
 * As long as reaper->stop is false, wait for new connections and
 * monitor any existing connetions for new pids to associate.  As
 * clients disconnect, the reaper on_disconnect function will be
 * called on each.
 *
 * @param reaper  - reaper instance
 */
void reaper_loop(struct reaper * reaper);

/*
 * Default handler for client disconnections.  This function will call
 * reaper_kill_client_pids() and reaper_unlink_client_paths() for the given
 * client.
 *
 * @param reaper  - reaper instance
 * @param client  - client instance
 */
void reaper_default_on_disconnect(struct reaper * reaper, struct client * client);

/*
 * Alternative handler for client disconnection.  This function will call
 * reaper_kill_client_pids() and reaper_unlink_client_paths() for all connected
 * clients and then force the reaper itself to exit.
 *
 * @param reaper  - reaper instance
 * @param client  - ignored
 */
void reaper_exit_on_disconnect(struct reaper * reaper, struct client * client);

__END_DECLS
#endif
