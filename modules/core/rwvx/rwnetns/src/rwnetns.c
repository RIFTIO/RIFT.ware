/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_netns.c
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 03/20/2014
 * @Fastpath network context functionality
 *
 * @details
 */
#define _GNU_SOURCE
#include <asm/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sched.h>
#include <rwlib.h>
#include <rwnetns.h>

#ifndef PATHMAX
#define PATHMAX 4096
#endif
pthread_mutex_t rwnetns_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * This function is called to get the fd of the current network namespace of the process.
 * Instead of opening the file here, we can add the fd into the tasklet.
 * @return fd value.
 */
int rwnetns_get_current_netfd()
{
  return open("/proc/self/ns/net",  O_RDONLY);
}

/**
 * This function is called to get the fd of network namespace given the name
 */
int rwnetns_get_netfd(const char *name)
{
  char netns_path[PATHMAX];
  RW_ASSERT(name);
  snprintf(netns_path, sizeof(netns_path), "%s/%s", RWNETNS_DIR, name);
  return open(netns_path, O_RDONLY);
}

/**
 * This function is called to delete a network namespace given the name
 * @param[in] name
 * @return 0 if success
 * @return errno if failed
 */
int rwnetns_delete_context(const char *name)
{
  int fd;
  char netns_path[PATHMAX];

  RWNETNS_LOCK();
  
  fd = rwnetns_get_netfd(name);
  if (fd > 0){
    close(fd);
    snprintf(netns_path, sizeof(netns_path), "%s/%s", RWNETNS_DIR, name);
    umount2(netns_path, MNT_DETACH);
    unlink(netns_path);
  }

  RWNETNS_UNLOCK();
  
  return 0;
}

/**
 * This function is called to create a namespace with the name. Once the new namespace
 * is created, the process is NOT in the new namespace. We need to fix the close of the
 * currfd once the currfd is stored in the tasklet information.
 * @param[in] name
 * @return 0 if success
 * @return errno if failed
 */
int rwnetns_create_context(const char *name)
{
  char netns_path[PATHMAX];
  int fd = 0;
  int currfd = 0;
  DIR *dir = NULL;
  static int dir_created = 0;
  int ret = 0;
  
  RWNETNS_LOCK();

  currfd  = rwnetns_get_current_netfd();
  
  if (!name || name[0] == 0){
    ret = 0;
    goto ret;
  }

  if (!dir_created){
    dir = opendir(RWNETNS_DIR);
    if (!dir){
      ret = mkdir(RWNETNS_DIR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
      if (ret){
        ret = errno;
        goto ret;
      }
      dir_created = 1;
    }else{
      dir_created = 1;
    }
  }
  
  snprintf(netns_path, sizeof(netns_path), "%s/%s", RWNETNS_DIR, name);
  fd = open(netns_path, O_RDONLY|O_CREAT|O_EXCL, 0);
  if (fd < 0) {
    ret = errno;
    goto ret;
  }
  if (unshare(CLONE_NEWNET) < 0) {
    ret = errno;
    umount2(netns_path, MNT_DETACH);
    goto ret;
  }
  {
    int sysctl_fd = open("/proc/sys/net/ipv6/conf/all/forwarding", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (sysctl_fd){
      write(sysctl_fd, "1\n", 2);
      close(sysctl_fd);
    }
  }
  /* Bind the netns last so I can watch for it */
  if (mount("/proc/self/ns/net", netns_path, "none", MS_BIND, NULL) < 0) {
    ret = errno;
    umount2(netns_path, MNT_DETACH);
  }
  setns(currfd, CLONE_NEWNET);
ret:
  if (currfd > 0){
    close(currfd);
  }
  if (fd > 0){
    close(fd);
  }
  if (dir){
    closedir(dir);
  }
  RWNETNS_UNLOCK();
  return ret;
}

/**
 * This function is called to open a socket in a given namespace
 * @param[in] namespace name
 * @param[in] domain
 * @param[in] type
 * @param[in] protocol
 * @return fd value
 */
int rwnetns_socket(const char *name, int domain, int type, int proto)
{
  int new_netfd = 0;
  int ret_fd = -1;
  int currfd = 0;
  int tried = 0;
  
  RWNETNS_LOCK();
  
  currfd = rwnetns_get_current_netfd();

tryagain:
  if (!name || name[0] == 0){
    new_netfd = 0;
  }else{
    new_netfd = rwnetns_get_netfd(name);
  }
  
  if (new_netfd > 0){
    setns(new_netfd, CLONE_NEWNET);
    ret_fd = socket(domain, type, proto);
    setns(currfd, CLONE_NEWNET);
  }else if (new_netfd < 0){
    if (!tried){
      RWNETNS_UNLOCK();
      rwnetns_create_context(name);
      RWNETNS_LOCK();
      tried = 1;
      goto tryagain;
    }else{
      goto ret;
    }
  }else if (new_netfd == 0){
    ret_fd = socket(domain, type, proto);
  }
  
ret:
  if (currfd > 0){
    close(currfd);
  }
  if (new_netfd > 0){
    close(new_netfd);
  }
  RWNETNS_UNLOCK();
  return ret_fd;
}




/**
 * This function is called to open a socket in a given namespace
 * @param[in] namespace name
 * @param[in] domain
 * @param[in] type
 * @param[in] protocol
 * @return fd value
 */
int rwnetns_open(const char *name, char *pathname, int flags)
{
  int new_netfd = 0;
  int ret_fd = -1;
  int currfd = 0;
  static int tried = 0;
  
  RWNETNS_LOCK();
  
  currfd = rwnetns_get_current_netfd();

tryagain:
  if (!name || name[0] == 0){
    new_netfd = 0;
  }else{
    new_netfd = rwnetns_get_netfd(name);
  }
  
  if (new_netfd > 0){
    setns(new_netfd, CLONE_NEWNET);
    ret_fd = open(pathname, flags);
    setns(currfd, CLONE_NEWNET);
  }else if (new_netfd < 0){
    if (!tried){
      RWNETNS_UNLOCK();
      rwnetns_create_context(name);
      RWNETNS_LOCK();
      tried = 1;
      goto tryagain;
    }else{
      goto ret;
    }
  }else if (new_netfd == 0){
    ret_fd = open(pathname, flags);
  }
  
ret:
  if (currfd > 0){
    close(currfd);
  }
  if (new_netfd > 0){
    close(new_netfd);
  }
  RWNETNS_UNLOCK();
  return ret_fd;
}

/**
 * This function is called whenever the tasklet wants to change its net namespace. The
 * caller must hold the mutex. We need to assert that...
 * @param[in] name
 * @return 0 if success
 * @return errno if failed
 */
int rwnetns_change(const char *name)
{
  int new_netfd = 0;
  static int tried = 0;
  int ret = -1;
  
  if (!name || name[0] == 0){
    return 0;
  }
tryagain:
  //AKKI assert that the pthread lock has been taken.
  new_netfd = rwnetns_get_netfd(name);
  if (new_netfd > 0){
    setns(new_netfd, CLONE_NEWNET);
    ret = 0;
  }else if (!tried){
    RWNETNS_UNLOCK();
    rwnetns_create_context(name);
    RWNETNS_LOCK();
    tried = 1;
    goto tryagain;
  }

  if (new_netfd > 0){
    close(new_netfd);
  }
  
  return ret;
}

/**
 * This function is called whenever the tasklet wants to change its net namespace. The caller must hold the pthread mutex. We need to assert that...
 * @param[in] fd
 * @return 0 if success
 * @return errno if failed
 */
int rwnetns_change_fd(int fd)
{
  //AKKI assert that the pthread lock has been taken.
  RW_ASSERT(fd > 0);
  setns(fd, CLONE_NEWNET);
  return 0;
}

/**
 * This function is called whenever the process wants to override it's default resolv.conf with a custom one.
 * @param[in] A new resolv.conf file to override the existing resolv.conf in this process
 * @return 0 if success
 * @return errno if failed
 */
int rwnetns_mount_bind_resolv_conf(const char *new_resolv_conf_path)
{
  int fd = open(new_resolv_conf_path, O_RDONLY, 0);
  int ret = 0;
  
  if (fd < 0) {
    return errno;
  }
  close(fd);


  // Make sure any future mounts do not affect any other process.
  if (unshare(CLONE_NEWNS) < 0) {
      return errno;
  }

  // Make sure the resolv.conf mount does not propogate upwards
  if (mount("", "/", "none", MS_SLAVE | MS_REC, NULL) < 0) {
      return errno;
  }

  if (mount(new_resolv_conf_path, RESOLV_CONF_PATH, "none", MS_BIND, NULL) < 0) {
    ret = errno;
    umount2(RESOLV_CONF_PATH, MNT_DETACH);
    return ret;
  }

  return 0;
}

/**
 * This function is called whenever the process wants to unmount the resolv.conf override.
 * @return 0 if success
 * @return errno if failed
 */
int rwnetns_unmount_resolv_conf(void)
{
  return umount2(RESOLV_CONF_PATH, MNT_DETACH);
}

void rwnetns_get_context_name(uint32_t fpathid, uint32_t contextid, char *name,
                              int siz)
{
  snprintf((name), siz,
           "%x-%x", (fpathid), (contextid));
}

char* rwnetns_get_gi_context_name(unsigned int fpathid, unsigned int contextid)
{
  char *context_name = NULL;
  asprintf(&context_name, "%x-%x", fpathid, contextid);
  return context_name;
}

void rwnetns_get_ip_device_name(uint32_t fpathid, uint32_t contextid,
                               char *commonname,
                               char *name, int siz)
{
  snprintf((name), siz,
          "rw%s%d-%x", (commonname), (fpathid), (contextid));
}


void rwnetns_get_device_name(uint32_t fpathid, uint32_t lportid,
                            char *name, int siz)
{
  snprintf((name), siz,
           "rw%d-%x", (fpathid), (lportid));
}
