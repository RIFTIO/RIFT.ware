
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


/**
 * @file rw_piot_setup.c
 * @author Matt Harper matt.harper@riftio.com
 * @date 02/24/2014
 * @brief Setup Linux environment for DPDK
 * 
 * @details 
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>
#include <fcntl.h>

#define ASSERT assert

#define PATH_NR_HUGEPAGES_2MB "/sys/devices/system/node/node%d/hugepages/hugepages-2048kB/nr_hugepages"
#define PATH_NR_HUGEPAGES_1GB "/sys/devices/system/node/node%u/hugepages/hugepages-1048576kB/nr_hugepages"
#define PATH_HUGEPAGES_MOUNTPOINT "/mnt/huge"
#define HUGEPAGE_MEMORY_RESERVE_PER_SOCKET (2*1024ULL*1024ULL*1024ULL)

int dpdk_system_setup(int no_huge, int req_memory);

static int check_module(const char *module_name);


static int
hpet_check(void)
{
  FILE *fp;
  char *s, buf[256] = {};

  fp = fopen("/proc/timer_list","r");
  if (NULL == fp) {
    return -1;
  }
  
  while(!feof(fp)) {
    s = fgets(buf,sizeof(buf),fp);
    if ((NULL != s) && (NULL != strstr(s,"hpet"))) {
      fclose(fp);
      return 0; // SUCCESS
    }
  }

  fclose(fp);
  return -2;
}


static int
check_module(const char *module_name)
{
  FILE *fp = NULL;
  char *s = NULL;
  int errors = 0;
  int rc, is_root = 0, found_module = 0;
  ASSERT(module_name);

  /*
   * Only root can load kernel modules
   */
  if (0 == geteuid()) {
    is_root = 1;
  }

  s = malloc(1024+strlen(module_name));
  ASSERT(s);

  fp = popen("/sbin/lsmod","r");
  if (NULL == fp) {
    errors++;
  }
  else {
    char *p, *p2, buf[2048];
    do {
      p = fgets(buf,sizeof(buf),fp);
      if (NULL != p) {
	p2 = strstr(p,module_name);
	if ((NULL != p2) && (p == p2)) {
	  found_module = 1;
	  break;
	}
      }
    } while (p);

    if (!found_module) {
      if (is_root) {
	sprintf(s,"/sbin/modprobe %s",module_name);
	rc = system(s);
	if (0 != rc) {
	  printf("\"%s\" failed - (Please perform manual \"insmod %s.ko\")\n",s,module_name);
	  errors++;
	}
	else {
	  found_module = 1;
	}
      }
      if (!found_module) {
	printf("Unable to load kernel module %s\n",module_name);
      }
    }
  }

  if (s) {
    free(s);
    s = NULL;
  }
  if (fp) {
    fclose(fp);
    fp = NULL;
  }
  return (0 == errors)?0:-1;
}



static int add_file_permissions(const char *filename, unsigned int perms_to_add)
{
  int rc, tries, errors = 0;
  struct stat stat_buf;
  char buf[256];
  ASSERT(filename);



  bzero(&stat_buf, sizeof(stat_buf));

  /*
   * Only root can modify file permissions
   */
  if (0 != geteuid()) {
    return 0; // FAKE SUCCESS
  }


  for(tries = 0;tries < 2;tries++) {
    rc = stat(filename,&stat_buf);
    if (0 != rc) {
      buf[0]=0; strerror_r(errno,buf,sizeof(buf));
      printf("Unable to stat(\"%s\") rc=%d errno=%d(%s)\n",filename,rc,errno,buf);
      errors++;
      break;
    }
    else {
      /* check if any permission adjustments are required */
      if ((stat_buf.st_mode & perms_to_add) == perms_to_add) {
	break;
      }
      if (tries > 0) {
	errors++; // previous adjust failed
	break;
      }
      // adjust permissions
      rc = chmod(filename,stat_buf.st_mode | perms_to_add);
      if (0 != rc) {
	buf[0]=0; strerror_r(errno,buf,sizeof(buf));
	printf("Unable to chmod(\"%s\") rc=%d errno=%d(%s)\n",filename,rc,errno,buf);
	errors++;
	break;
      }
    }
  }
  
  return (0 == errors) ?0:-1;
}

static int adjust_proc_setting(char *valpath, int newval)
{
  int rc,val,tries, errors=0;
  ASSERT(valpath);

  for(tries=0;tries<2;tries++) {
    FILE *fp = NULL;
    fp = fopen(valpath,"r");
    if (fp) {
      rc = fscanf(fp,"%d",&val);
      fclose(fp);
      if (1 != rc) {
	errors++;
	break;
      }
      if (val >= newval) {
	break;
      }

      if (0 != geteuid()) {
	goto done; // Only root can modify /proc - FAKE SUCCESS
      }
      fp = fopen(valpath,"w");
      if (fp) {
	rc = fprintf(fp,"%d",newval);
	fclose(fp);
	if (1 != rc) {
	  errors++;
	  break;
	}
      }
    }
  }

done:
  return (0 == errors)?0:-1;
}


/**
 *
 * This function attempts to prepare the system to run DPDK
 */
int dpdk_system_setup(int no_huge, int req_memory)
{
  int rc, nsocket;
  unsigned int is_root = 0, errors = 0;
  uid_t euid;
  char fn[256] = {}, special_mount_options[256] = {};
  struct stat stat_buf;
  long long huge_page_mem_per_socket;


  euid = geteuid();
  if (euid == 0) {
    is_root = 1;
  }
  else {
    printf("NOT Running as root - WARNING: many config steps being skipped...\n");
  }


  rc = hpet_check();
  if (rc) {
    printf("HPET not enabled - need to enable HPET_MMAP in kernel and/or BIOS\n");
    printf("skipping for now...\n");
    //errors++;
  }
  else {
    /*
     * DPDK can be run non-ROOT if file permissions are setup properly:
     *      /dev/hpet
     *      /mnt/huge
     *      /dev/uio0, /dev/uio1, etc.
     */
    rc = add_file_permissions("/dev/hpet",0666);  
    if (0 != rc) {
      errors++;
    }
  }

  /*
   * Change permissions for all /dev/uio* files
   */
  {
    DIR *dir;
    struct dirent *dent;
    dir = opendir("/dev");
    if (NULL != dir) {
      while(NULL != (dent=readdir(dir))) {
	if ((!strncmp("uio",dent->d_name,3) && (strlen(dent->d_name) > 3)) ||
              (!strcmp("hugepages", dent->d_name))) {
	  size_t buflen = strlen(dent->d_name)+strlen("/dev/")+1;
	  char *uioname = malloc(buflen);
	  ASSERT(uioname);
	  snprintf(uioname,buflen,"/dev/%s",dent->d_name);
	  rc = add_file_permissions(uioname,0777);
	  if (0 != rc) {
	    errors++;
	  }
	  free(uioname);
	}
      }
      closedir(dir);
    }
    else {
      errors++;
    }
  }

  
  /*
   * install all required loadable modules
   * This assume the modules have been properly installed in the system; otherwise,
   * we would need to explicitly perform an insmod() on the .ko file
   */
  rc = check_module("uio");
  if (0 != rc) {
    errors++;
  }
  rc = check_module("igb_uio");
  if (0 != rc) {
    errors++;
  }
  rc = check_module("rte_kni");
  if (0 != rc) {
    errors++;
  }


  if (no_huge) {
    goto  skip_huge_page;
  }

  huge_page_mem_per_socket = HUGEPAGE_MEMORY_RESERVE_PER_SOCKET;
  if (req_memory) {
    huge_page_mem_per_socket = req_memory * 1024ULL*1024ULL;
  }

  /*
   * Determine if system is configured for 2MB or 1GB Hugepages and if required, configure the hugepages
   */
  for(nsocket=0;nsocket<4;nsocket++) {
    int npages;
    sprintf(fn,PATH_NR_HUGEPAGES_2MB,nsocket);
    memset(&stat_buf,0,sizeof(stat_buf));
    rc = stat(fn,&stat_buf);
    if (0 == rc) {
      int npages;
      printf("Found 2MB hugepages on socket%d\n",nsocket);
      npages = huge_page_mem_per_socket/(2ULL*1024ULL*1024ULL);
      if (npages < 1) {
	npages = 1;
      }
      rc = adjust_proc_setting(fn,npages);
    }
    else {
      sprintf(fn,PATH_NR_HUGEPAGES_1GB,nsocket);
      memset(&stat_buf,0,sizeof(stat_buf));
      rc = stat(fn,&stat_buf);
      if (0 == rc) {
	printf("Found 1GB hugepages on socket%d\n",nsocket);
	strcpy(special_mount_options,"pagesize=1GB");
	npages = huge_page_mem_per_socket/(1024ULL*1024ULL*1024ULL);
	if (npages < 1) {
	  npages = 1;
	}
	rc = adjust_proc_setting(fn,npages);
      }
      else {
	if (0 == nsocket) {
	  printf("No hugepages found!!!\n");
	  errors++;
	}
	else {
	  break; // All sockets checked
	}
      }
    }
  }
  

  /*
   * Ensure hugepages are mounted
   */
  if (is_root) {
    sprintf(fn,PATH_HUGEPAGES_MOUNTPOINT);
    memset(&stat_buf,0,sizeof(stat_buf));
    rc = stat(fn,&stat_buf);
    if (0 == rc) {
      //printf("Hugepage mountpoint exists\n");
    }
    else {
      rc = mkdir(fn,0777);
      if (rc != 0) {
	printf("mkdir() failed\n");
	errors++;
	goto done;
      }
    }

  rc = add_file_permissions(PATH_HUGEPAGES_MOUNTPOINT,0777);
  if (0 != rc) {
    errors++;
  }

    /*
     * Try to re-mount hugetlbfs
     */
    rc = mount("",fn,"hugetlbfs",MS_NODEV|MS_REMOUNT,(const void *)special_mount_options);
    if (0 == rc) {
      //printf("Remount seemed to have been successful\n");
    }
    else {
      /*
       * Try to mount hugetlbfs
       */
      rc = mount("",fn,"hugetlbfs",MS_NODEV,(const void *)special_mount_options);
      if (rc < 0) {
	printf("mount() hugetblfs failed rc=%d\n",rc);
	goto done;
      }
    }
  }

skip_huge_page:
  
  if (0 == geteuid()) {
    int fd, ret;

    /*Turn on core dump filter to dump shared huge pages*/
    {
      pid_t processid = getpid();
      char name[64];
      sprintf(name, "/proc/%u/coredump_filter", processid);
      fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0666);
      if (fd > 0){
        ret = write(fd, "127\n", 4);
        if (ret <= 0){
          errors++;
        }
        close(fd);
      }
    }
    
    /*Turn on ip-forwarding*/
    fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "1\n", 2);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }
    fd = open("/proc/sys/net/ipv6/conf/all/forwarding", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "1\n", 2);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }
    fd = open("/proc/sys/net/ipv6/conf/default/forwarding", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "1\n", 2);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }
    /*Turn off rp-filter*/
    fd = open("/proc/sys/net/ipv4/conf/all/rp_filter", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "0\n", 2);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }
    fd = open("/proc/sys/net/ipv4/conf/default/rp_filter",
              O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "0\n", 2);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }

    
    
    /*Turn on arp-filter even though we do not do source-based routing. We need to
     actually control this using arp_announce and arp_ignore*/
    fd = open("/proc/sys/net/ipv4/conf/all/arp_filter", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "1\n", 2);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }
    fd = open("/proc/sys/net/ipv4/conf/default/arp_filter",
              O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "1\n", 2);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }
    fd = open("/proc/sys/net/ipv4/conf/default/arp_accept",
              O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "1\n", 1);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }
    fd = open("/proc/sys/net/ipv4/conf/all/arp_accept",
              O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd > 0){
      ret = write(fd, "1\n", 1);
      if (ret <= 0){
        errors++;
      }
      close(fd);
    }
  }
  
 done:
  if (errors == 0) {
    return 0;
  }
  return -1;
}


/*
 * MISC NOTES:
 * isolcpus kernel boot args example: isolcpus=2,4,6
 */
