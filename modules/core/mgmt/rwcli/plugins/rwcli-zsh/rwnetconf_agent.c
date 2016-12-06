
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
 * @file rwnetconf_agent.c 
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 09/28/2015
 * @brief Netconf agent for the rwcli. Launches python based netconf agent. 
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#include <sys/prctl.h>
#include <termios.h>

#undef _GNU_SOURCE

#include "rwcli_zsh.h"
#include "rwcli_agent.h"

#include "rwnetconf_agent.mdh"

#define RW_NETCONF_AGENT_PATH "/usr/bin/rwnetconf_agent.py"

extern rift_cmdargs_t rift_cmdargs;

/* Module Features. We don't have any. */
static struct features module_features = {
  NULL, 0,
  NULL, 0,
  NULL, 0,
  NULL, 0,
  0
};

/*
 * Launch the Netconf python agent and pass the channel fds's and other command
 * line arguments.
 */
static void launch_netconf_agent()
{
  // Launch the python process

  int pid = fork();
  switch(pid) {
    case 0: {
      int ret = 0;
      char read_channel_fd_arg[8];
      char write_channel_fd_arg[8];
      char trace_level_str[8];
      char* rift_install = NULL;
      char* agent_path = NULL;
      int argc = 0;
      char *argv[16];
      int fd = 0;
      int in_fd = -1;
      int out_fd = -1;

      in_fd = rwcli_controller.agent_channels[RWCLI_TRANSPORT_MODE_NETCONF].in.fd.read;
      out_fd = rwcli_controller.agent_channels[RWCLI_TRANSPORT_MODE_NETCONF].out.fd.write;

      // if the parent exits the child also exits
      ret = prctl(PR_SET_PDEATHSIG, SIGTERM);
      RW_ASSERT(ret == 0);

      snprintf(read_channel_fd_arg, 8, "%d", in_fd);
      snprintf(write_channel_fd_arg, 8, "%d", out_fd);
      snprintf(trace_level_str, 8, "%d", rift_cmdargs.trace_level);

      // Set the path for netconf python script
      rift_install = getenv("RIFT_INSTALL");
      if (rift_install) {
        asprintf(&agent_path, "%s%s", rift_install, RW_NETCONF_AGENT_PATH);
      } else {
        agent_path = strdup(RW_NETCONF_AGENT_PATH);
      }
      
      argv[argc++] = "/usr/bin/python3";
      argv[argc++] = agent_path;
      argv[argc++] = "--read-channel-fd";
      argv[argc++] = read_channel_fd_arg;
      argv[argc++] = "--write-channel-fd";
      argv[argc++] = write_channel_fd_arg;
      if (rift_cmdargs.netconf_host) {
        argv[argc++] = "--netconf-host";
        argv[argc++] = rift_cmdargs.netconf_host;
      }
      if (rift_cmdargs.netconf_port) {
        argv[argc++] = "--netconf-port";
        argv[argc++] = rift_cmdargs.netconf_port;
      }

      if (rift_cmdargs.username) {
        argv[argc++] = "--username";
        argv[argc++] = rift_cmdargs.username;
      } 
      if (rift_cmdargs.passwd) {
        argv[argc++] = "--passwd";
        argv[argc++] = rift_cmdargs.passwd;
      } 

      if (rift_cmdargs.trace_level != RWTRACE_SEVERITY_ERROR) {
        argv[argc++] = "--trace-level";
        argv[argc++] = trace_level_str;
      }
      argv[argc] = NULL;

      // Close the FDs that are not required
      for (fd = 3; fd < 65535; fd++) {
        if ((fd != in_fd) &&
            (fd != out_fd)) {
          close(fd);
        }
      }

      ret = execv(argv[0], argv);
      if (ret != 0) {
        // Log an error
        free(agent_path);
      }
      exit(-1);
    }
    case -1:
      // Log an error
      break;
    default: {
    }
  }

}

/* The empty comments have special meaning. the ZSH generators use them for
 * exporting the methods */

/* This function is called when the module is loaded */

/**/
int
setup_(UNUSED(Module m))
{
  return 0;
}

/* This function is called after the setup, to get the capabilities of this
 * module. For RIFT we are not exposing any features. */

/**/
int
features_(Module m, char ***features)
{
  *features = featuresarray(m, &module_features);
  return 0;
}

/* This is called from within the featuresarray to enable the features */

/**/
int
enables_(Module m, int **enables)
{
  return handlefeatures(m, &module_features, enables);
}

/* Called after the features are enabled when the module is loaded.
 * Module initialization code goes here. */

/**/
int
boot_(Module m)
{
  launch_netconf_agent(); 
  fflush(stdout);

  return 0;
}

/* Called when the module is about to be unloaded. 
 * Cleanup used resources here. */

/**/
int
cleanup_(Module m)
{
  return setfeatureenables(m, &module_features, NULL);
}

/* Called when the module is unloaded. */

/**/
int
finish_(UNUSED(Module m))
{
  return 0;
}

