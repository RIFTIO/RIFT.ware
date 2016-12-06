
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

#ifndef __REAPER_CLIENT_H__
#define __REAPER_CLIENT_H__

#include <sys/time.h>
#include <sys/types.h>

__BEGIN_DECLS

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

/*
 * Connect to a reaper using the specified path.  Connection attempts will
 * continue until either successful connection is made, or if specified, the
 * timeout elapses.
 *
 * @param path    - path to the reaper's local unix socket
 * @param timeout - if not NULL, maximum amount of time to wait for a
 *                  successful connnection
 * @return        - connection file descriptor on success, negative errno on
 *                  failure
 */
int reaper_client_connect(const char * path, struct timeval * timeout);

/*
 * Disconnect from a reaper
 *
 * @param reaper_fd - reaper socket connection
 */
void reaper_client_disconnect(int reaper_fd);

/*
 * Add a pid to the reaper.  When the reaper connection is closed, the
 * specified process will be reaped.
 *
 * @param reaper_fd - reaper socket
 * @param pid       - pid to be reaped when connection is closed
 * @return          - 0 on success, -1 otherwise
 */
int reaper_client_add_pid(int reaper_fd, pid_t pid);

/*
 * Del a pid from the reaper.  
 *
 * @param reaper_fd - reaper socket
 * @param pid       - pid to be removed 
 * @return          - 0 on success, -1 otherwise
 */
int reaper_client_del_pid(int reaper_fd, pid_t pid);

/*
 * Add a path to the reaper.  When the reaper connection is closed, the
 * specified path will be unlinked.
 *
 * @param reaper_fd - reaper socket
 * @param path      - path to be unlinked when connection is closed
 * @return          - 0 on success, -1 otherwise
 */
int reaper_client_add_path(int reaper_fd, const char * path);

__END_DECLS
#endif
