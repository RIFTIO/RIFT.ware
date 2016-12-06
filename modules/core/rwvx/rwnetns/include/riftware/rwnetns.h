
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
 */


/*!
 * @file rw_netlink.h
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 03/20/2014
 * @brief Netlink library to be used by applications to receive/send netlink/ioctls
 *
 * @details
 */
#ifndef __RWNETNS_H__
#define __RWNETNS_H__

#include <stdint.h>

#include <glib.h>
#include <glib-object.h>

#ifndef __GI_SCANNER__
__BEGIN_DECLS
#endif /* __GI_SCANNER__ */

#define RW_MAX_NETNS_NAME 64
// extern pthread_mutex_t rwnetns_mutex;
// #define RWNETNS_LOCK()    pthread_mutex_lock(&rwnetns_mutex)
// #define RWNETNS_UNLOCK()  pthread_mutex_unlock(&rwnetns_mutex)

#define RWNETNS_LOCK()
#define RWNETNS_UNLOCK()
#define RWNETNS_DIR "/var/run/netns"
#define RESOLV_CONF_PATH "/etc/resolv.conf"

void rwnetns_get_context_name(uint32_t fpathid, uint32_t contextid, char *name,
                             int siz);
char* rwnetns_get_gi_context_name(unsigned int fpathid, unsigned int contextid);
void rwnetns_get_ip_device_name(unsigned int fpathid, unsigned int contextid,
                               char *commonname,
                               char *name, int siz);
void rwnetns_get_device_name(unsigned int fpathid, unsigned int contextid,
                            char *name, int siz);

int rwnetns_get_netfd(const char *name);
int rwnetns_get_current_netfd();
int rwnetns_create_context(const char *name);
int rwnetns_socket(const char *name, int family, int domain, int proto);
int rwnetns_delete_context(const char *name);
int rwnetns_change_fd(int fd);
int rwnetns_change(const char *name);
int rwnetns_mount_bind_resolv_conf(const char *new_resolv_conf_path);
int rwnetns_unmount_resolv_conf();
int rwnetns_open(const char *name, char *pathname, int flags);

#ifndef __GI_SCANNER__
__END_DECLS
#endif /* __GI_SCANNER__ */

#endif /* __RWNETNS_H__ */
