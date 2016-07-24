
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


#include <getopt.h>
#include <unistd.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>

#include <rwdts.h>
#include <rwmsg_clichan.h>
#include <rwtasklet.h>
#include <rwtrace.h>
#include <rwvcs.h>
#include <rwvcs_manifest.h>
#include <rwvcs_rwzk.h>

#include <google/coredumper.h>

#include "rwdts_int.h"

#include "rwmain.h"

static void setup_inotify_watcher(rwvx_instance_ptr_t rwvx);

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#define PACEMAKER_CRM_MON_DIR "/tmp/corosync/"
#define PACEMAKER_CRM_MON_FNAME "crm_mon.html"
#define PACEMAKER_CRM_MON_FULL_FNAME PACEMAKER_CRM_MON_DIR PACEMAKER_CRM_MON_FNAME
//"/tmp/corosync/crm_mon.html"

rw_status_t
read_pacemaker_state(rwmain_pm_struct_t *pm)
{
  char buffer[BUF_LEN];
  char tmp[999];
  bool file_read = false;
  RW_ASSERT(pm);

  // Read the pacemaker status
  sprintf(tmp,
          "sudo --non-interactive cat %s",
          PACEMAKER_CRM_MON_FULL_FNAME);
  FILE *fp = popen(tmp, "r");
  buffer[0] = 0;
  while (fgets(tmp, sizeof(tmp), fp) != NULL ) {
    strcat(buffer, tmp);
    file_read = true;
  }
  fclose(fp);

  #define FIND_VALUE(b_ptr, pat_str, s_char, e_char) \
  ({ \
    char *p1, *p2; \
    if (pat_str[0]!='\0') {  \
      p1 = strstr(b_ptr, pat_str); if (!p1) goto _done; \
      p1 += strlen(pat_str); \
    } else {  \
      p1 = b_ptr;  \
    } \
    p1 = strchr(p1, s_char); if (!p1) goto _done;  \
    p1++; \
    p2 = strchr(p1, e_char); if (!p2) goto _done;  \
    p2[0] = '\0';  \
    p2++; \
    b_ptr = p2; \
    &(p1[0]); \
  })

  if (file_read) {
    memset(pm, '\0', sizeof(*pm));
    char *buf_ptr;
    struct in_addr addr;
    buf_ptr = &(buffer[0]);
    char *dc_name = FIND_VALUE(buf_ptr, "Current DC", ' ', ' ');
    char my_hostname[999];
    int r = gethostname(my_hostname, sizeof(my_hostname));
    RW_ASSERT(r != -1);
    if (!strcmp(my_hostname, dc_name)) {
      pm->i_am_dc = true;
    }
    char *dc_ip = FIND_VALUE(buf_ptr, "", '(', ')');
    addr.s_addr = htonl(atoi(dc_ip));
    fprintf(stderr, "My Host Name: *%s* Current DC : *%s* %s\n", my_hostname, dc_name, inet_ntoa(addr));
    int i = 0;
    while (1) {
      char *node_ip = FIND_VALUE(buf_ptr, "Node:", '(', ')');
      addr.s_addr = htonl(atoi(node_ip));

      char *state = FIND_VALUE(buf_ptr, "", '>', '<');

      pm->ip_entry[i].pm_ip_addr = strdup(inet_ntoa(addr));
      if (!strcmp(state,"OFFLINE")) {
          pm->ip_entry[i].pm_ip_state = RWMAIN_PM_IP_STATE_FAILED;
      } else if (!strcmp(state, "online")) {
        if (!strcmp(dc_ip, node_ip)) {
          pm->ip_entry[i].pm_ip_state = RWMAIN_PM_IP_STATE_LEADER;
        } else {
          pm->ip_entry[i].pm_ip_state = RWMAIN_PM_IP_STATE_ACTIVE;
        }
      } else {
        fprintf(stderr, "pacemaker: Node=%s State=%s\n", node_ip, state);
        RW_CRASH();
      }
      i++;
    }
  } else {
    return RW_STATUS_FAILURE;
  }
_done:
  return RW_STATUS_SUCCESS;
}

static void pacemaker_state_changed(
    rwsched_CFSocketRef s,
    CFSocketCallBackType type,
    CFDataRef address,
    const void *data,
    void *info)
{
  rwvx_instance_ptr_t rwvx = (rwvx_instance_ptr_t)info;
  char buffer[BUF_LEN];

  rwmain_pm_struct_t pm = {};
  static rwmain_pm_struct_t prev_pm = {};

  // Read the event fd; otherwise event keeps comimng
  read(rwvx->pacemaker_inotify.fd, buffer, BUF_LEN);

  // Read the pacemaker status
  if (read_pacemaker_state(&pm) != RW_STATUS_SUCCESS) {
    goto _done;
  }

  if (pm.i_am_dc != prev_pm.i_am_dc
      || pm.ip_entry[0].pm_ip_state != prev_pm.ip_entry[0].pm_ip_state
      || pm.ip_entry[1].pm_ip_state != prev_pm.ip_entry[1].pm_ip_state
      || strcmp(pm.ip_entry[0].pm_ip_addr, prev_pm.ip_entry[0].pm_ip_addr)
      || strcmp(pm.ip_entry[1].pm_ip_addr, prev_pm.ip_entry[1].pm_ip_addr)) {
    NEW_DBG_PRINTS(
            "State changed\n"
            " .i_am_dc = %u\n"
            " .ip_entry = {\n"
            "\t {\n"
            "\t\t .pm_ip_addr = \"%s\",\n"
            "\t\t .pm_ip_state = %u\n"
            "\t },\n"
            "\t {\n"
            "\t\t .pm_ip_addr = \"%s\",\n"
            "\t\t .pm_ip_state = %u\n"
            "\t },\n"
            " } %s\n\n",
            pm.i_am_dc,
            pm.ip_entry[0].pm_ip_addr,
            pm.ip_entry[0].pm_ip_state,
            pm.ip_entry[1].pm_ip_addr,
            pm.ip_entry[1].pm_ip_state,
            rwvx->rwmain?rwvx->rwvcs->instance_name:"");
    if (rwvx->rwmain) {
      rwmain_pm_handler(rwvx->rwmain, &pm);
    }
  }
  free(prev_pm.ip_entry[0].pm_ip_addr);
  free(prev_pm.ip_entry[1].pm_ip_addr);
  prev_pm = pm;
_done:
  setup_inotify_watcher(rwvx);
}


static void setup_inotify_watcher(rwvx_instance_ptr_t rwvx)
{
  RW_ASSERT(rwvx);

  rwvx->pacemaker_inotify.fd = inotify_init();
  RW_ASSERT(rwvx->pacemaker_inotify.fd >= 0);
  int inotify_wd = inotify_add_watch(
      rwvx->pacemaker_inotify.fd,
      "/tmp/corosync",
      IN_MODIFY);
  RW_ASSERT(inotify_wd >= 0);

  CFSocketContext cf_context = { 0, rwvx, NULL, NULL, NULL };
  CFOptionFlags cf_callback_flags = kCFSocketReadCallBack;
  CFOptionFlags cf_option_flags = kCFSocketAutomaticallyReenableReadCallBack;

  // Release cfsource
  if (rwvx->pacemaker_inotify.cfsource) {
    RW_CF_TYPE_VALIDATE(rwvx->pacemaker_inotify.cfsource, rwsched_CFRunLoopSourceRef);
    rwsched_tasklet_CFSocketReleaseRunLoopSource(
        rwvx->rwsched_tasklet,
        rwvx->pacemaker_inotify.cfsource);
  }
  // Release the cfsocket
  if (rwvx->pacemaker_inotify.cfsocket) {
    RW_CF_TYPE_VALIDATE(rwvx->pacemaker_inotify.cfsocket, rwsched_CFSocketRef);

    // Invalidate the cfsocket
    rwsched_tasklet_CFSocketInvalidate(
        rwvx->rwsched_tasklet,
        rwvx->pacemaker_inotify.cfsocket);

    // Release the cfsocket
    rwsched_tasklet_CFSocketRelease(
        rwvx->rwsched_tasklet,
        rwvx->pacemaker_inotify.cfsocket);
  }

  rwvx->pacemaker_inotify.cfsocket = rwsched_tasklet_CFSocketCreateWithNative(
      rwvx->rwsched_tasklet,
      kCFAllocatorSystemDefault,
      rwvx->pacemaker_inotify.fd,
      cf_callback_flags,
      pacemaker_state_changed,
      &cf_context);
  RW_CF_TYPE_VALIDATE(rwvx->pacemaker_inotify.cfsocket, rwsched_CFSocketRef);

  rwsched_tasklet_CFSocketSetSocketFlags(
      rwvx->rwsched_tasklet,
      rwvx->pacemaker_inotify.cfsocket,
      cf_option_flags);
  
  rwvx->pacemaker_inotify.cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(
      rwvx->rwsched_tasklet,
      kCFAllocatorSystemDefault,
      rwvx->pacemaker_inotify.cfsocket,
      0);
  RW_CF_TYPE_VALIDATE(rwvx->pacemaker_inotify.cfsource, rwsched_CFRunLoopSourceRef);
  
  rwsched_tasklet_CFRunLoopAddSource(
      rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwvx->rwsched_tasklet),
      rwvx->pacemaker_inotify.cfsource,
      rwvx->rwsched->main_cfrunloop_mode);
}

void start_pacemaker_and_determine_role(
    rwvx_instance_ptr_t rwvx,
    vcs_manifest       *pb_rwmanifest,
    rwmain_pm_struct_t *pm)
{
  RW_ASSERT(rwvx);
  RW_ASSERT(pb_rwmanifest);
  RW_ASSERT(pm);

  if (pb_rwmanifest->bootstrap_phase->n_ip_addrs_list == 1) {
    pm->i_am_dc = true;
    return;
  }

  #define COROSYNC_CONF_WO_NODES \
    "totem {\n" \
      "\tversion: 2\n" \
      "\tcrypto_cipher: none\n" \
      "\tcrypto_hash: none\n" \
      "\tinterface {\n" \
        "\t\t ringnumber: 0\n" \
        "\t\t bindnetaddr: %s\n" \
        "\t\t mcastport: 5405\n" \
        "\t\t ttl: 1\n\t}\n" \
      "\ttransport: udpu\n}\n" \
    "quorum {\n\tprovider: corosync_votequorum\n}\n" \
    "nodelist {\n"

  #define COROSYNC_CONF_NODE_TEMPLATE \
      "\tnode {\n\t\tring0_addr: %s\n\t}\n"

      //"\tnode {\n\t\tring0_addr: %s\n\t\tnodeid: %u\n\t}\n"

  // Form the corosync_conf to be written to /etc/corosync/corosync.conf
  char corosync_conf[999];
  char *local_mgmt_addr = rwvcs_manifest_get_local_mgmt_addr(pb_rwmanifest->bootstrap_phase);
  RW_ASSERT(local_mgmt_addr);
  sprintf(corosync_conf, COROSYNC_CONF_WO_NODES, local_mgmt_addr);
  if (pb_rwmanifest->bootstrap_phase
      && pb_rwmanifest->bootstrap_phase->ip_addrs_list) {
    RW_ASSERT(pb_rwmanifest->bootstrap_phase->n_ip_addrs_list);
    int i; for (i=0; i<pb_rwmanifest->bootstrap_phase->n_ip_addrs_list; i++) {
      RW_ASSERT(pb_rwmanifest->bootstrap_phase->ip_addrs_list[i]);
      char corosync_conf_node[999];
      sprintf(corosync_conf_node
              ,COROSYNC_CONF_NODE_TEMPLATE
              ,pb_rwmanifest->bootstrap_phase->ip_addrs_list[i]->ip_addr);
              //, i+1);
      strcat(corosync_conf, corosync_conf_node);
    }
  }
  strcat(corosync_conf, "}");
  fprintf(stderr, "Witing to /etc/corosync/corosync.conf\n%s\n", corosync_conf);

  char command[999];
  int r = 0;
  sprintf(command,
          "echo \"%s\" > /tmp/blah;"
          "sudo mv /tmp/blah /etc/corosync/corosync.conf;"
          "sudo chown root /etc/corosync/corosync.conf;"
          "sudo chgrp root /etc/corosync/corosync.conf",
          corosync_conf);
  FILE *fp = popen(command, "r");
  fclose(fp);

#if 0
  pid_t pid;
  char * argv[8];
  int argc = 0;
  pid = fork();
  if (pid < 0) {
    RW_CRASH();
  }
  if (pid == 0) {
    argv[argc++] = "/bin/sudo";
    argv[argc++] = "--non-interactive";
    argv[argc++] = "/usr/sbin/pcs";
    argv[argc++] = "cluster";
    argv[argc++] = "stop";
    argv[argc++] = NULL;
    execv(argv[0], argv);
  }
#else
  fp = popen("sudo pkill crm_mon; sudo pcs cluster stop", "r");
  fclose(fp);
#endif

  usleep(2000000);
  // FIXME: dirty way to stop pacemaker service when rwmain exits
  if ((fork()) == 0) {
    setsid();
    signal(SIGCHLD, SIG_DFL);
    if ((fork()) == 0) {
      sprintf(command,
              "while [ 1 ]; do if ! (ps -A | grep -v %u | grep -q rwmain) 2> /dev/null; then sudo pcs cluster stop; exit; fi; sleep 1; done; sudo pkill crm_mon; sudo rm -f %s 2> /dev/null; rmdir /tmp/corosync/ 2> /dev/null",
              getpid(),
              PACEMAKER_CRM_MON_FULL_FNAME);
      fp = popen(command, "r");
      fclose(fp);
      exit(0);
    } else {
      exit(0);
    }
  }

#if 0
  pid = fork();
  if (pid < 0) {
    RW_CRASH();
  }
  if (pid == 0) {
    argv[argc++] = "/bin/sudo";
    argv[argc++] = "--non-interactive";
    argv[argc++] = "/usr/sbin/pcs";
    argv[argc++] = "cluster";
    argv[argc++] = "start";
    argv[argc++] = NULL;
    execv(argv[0], argv);
  }
#else
  fp = popen("sudo pcs cluster start", "r");
  fclose(fp);
#endif
  char *pacemaker_dc_found = NULL;
  while (1) {
    usleep(2000000);
    FILE *fp;
    fp = popen("sudo pcs status xml", "r");
    char buff[999];
    while (fgets(buff, sizeof(buff), fp) != NULL ) {
      //fprintf(stderr,"%s", buff);
      if ((pacemaker_dc_found = strstr(buff, "current_dc present=\"true\""))) {
        break;
      }
    }
    fclose(fp);
    if (pacemaker_dc_found) {
      char my_hostname[999];
      char *pacemaker_dc_name = NULL;
      char *pacemaker_dc_name_end = NULL;
      r = gethostname(my_hostname, sizeof(my_hostname));
      RW_ASSERT(r != -1);
      pacemaker_dc_name = strstr(pacemaker_dc_found, " name=\"");
      RW_ASSERT(pacemaker_dc_name != NULL);
      pacemaker_dc_name += sizeof(" name=");
      pacemaker_dc_name_end = strstr(pacemaker_dc_name, "\"");
      RW_ASSERT(pacemaker_dc_name_end != NULL);
      pacemaker_dc_name_end[0] = '\0';
      if (!strcmp(my_hostname, pacemaker_dc_name)) {
        fprintf(stderr,"I AM pacemaker DC\n");
      } else {
        fprintf(stderr,"I'm NOT pacemaker DC - my_hostname=%s pacemaker_dc_name=%s\n", my_hostname, pacemaker_dc_name);
      }
      break;
    }
  }

  /*setup a inotify watcher for /tmp/crm_mon.html */
  sprintf(command, "mkdir %s", PACEMAKER_CRM_MON_DIR);
  fp = popen(command, "r");
  fclose(fp);

  sprintf(command, "sudo crm_mon --daemonize --as-html %s", PACEMAKER_CRM_MON_FULL_FNAME);
  fp = popen(command, "r");
  fclose(fp);

  usleep(20000);
  setup_inotify_watcher(rwvx);

  read_pacemaker_state(pm);
}
