
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
 * @file rwtermio.c
 * @author Balaji Rajappa 
 * @date 08/25/2015
 * @brief Implementaion for Terminal IO tasklet. Creates a Pseudo terminal for
 * use with any interactive Native process. When the VCS is run in collapsed
 * mode under GDB, this tasklet facilitates the VCS to take control of the
 * terminal and relays the stdin to a pseudo terminal. The interactive Native
 * process should be attached to this pseudo terminal. 
 */

#define _GNU_SOURCE

#include <rwtermio.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <limits.h>
#include "rwvcs_rwzk.h"


#define RWTERMIO_READ_SIZE   256
#define RWTERMIO_WRITE_SIZE  1024

#define CTRL_C_CHAR 0x03

/*
 * Pipe used for posting the sigwinch signal as a rwsched callback instead of a
 * handling the sigwinch in a signal handler
 */
static int sigwinch_pipe[2];

/*
 * Determines if GDB is attached. 
 * @returns Returns true if gdb is attached else returns false
 */
static bool 
is_gdb_attached()
{
  const int line_size = 256;
  char line[line_size];
  const char* proc_status = "/proc/self/status";
  const char* tracer_pid = "TracerPid:";
  const int tracer_pid_len = strlen(tracer_pid);

  FILE* fp = fopen(proc_status, "r");
  if (fp == NULL) {
    return false;
  }

  while(fgets(line, line_size, fp)) {
    if (strstr(line, tracer_pid) != NULL) {
      char *pid_str = line + tracer_pid_len;
      int pid = atoi(pid_str);
      if (pid == 0) {
        return false;
      }
      return true;
    }
  }
  return false;
}


/*
 * Signal handler for the SIGWINCH terminal window resize signal. This is called
 * in the signal handler context. This method posts an event on sigwich_pipe
 * fd, so that the signal is handled in rwsched_CFRunLoop context
 * @param tasklet   - rwsched tasklet information
 * @param signo     - signal number (SIGWINCH) in this case
 */
static void
rwtermio_signwinch_handler(rwsched_tasklet_ptr_t tasklet, int signo)
{
  char buf[1] = { 0 };
  UNUSED(tasklet);
  RW_ASSERT(signo == SIGWINCH);
  
  // Wakeup the rwsched_CFRunLoop to handle read event on sigwinch_pipe,
  // ioctl are not safe to be invoked from signal handlers 
  // (also debug print statements cannot be added to the signal handlers)
  write(sigwinch_pipe[1], buf, 1);
}

/*
 * Gets the terminal size of the rwmain terminal and synchronize this size with
 * the pseudo terminal.
 * @param instance  - Terminal IO takslet instance pointer
 */
static void 
rwtermio_sync_window_size(rwtermio_instance_ptr_t instance)
{
  int ret = 0;
  struct winsize wsize;

  if (!isatty(STDIN_FILENO)) {
    /* Stdin not a TTY, return failure. This might happen when riftware run
     * without a terminal and RW.CLI is part of the manifest. */
    return;
  }


  ret = ioctl(STDIN_FILENO, TIOCGWINSZ, &wsize);
  if (ret != 0) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to Get Terminal Window Size: %s", strerror(err));
    return;
  }

  ret = ioctl(instance->master_fd, TIOCSWINSZ, &wsize);
  if (ret != 0) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to Set Terminal Window Size: %s", strerror(err));
    return;
  }
}

/*
 * RW.Sched callback on receiving the sigwinch signal. The resized terminal size
 * is synced with the Pseudo terminal. Since the underlying Native process is an
 * interactive one, it needs to know the terminal size to repaint its window
 * correctly.
 * @param info  - points to the TerminalIO instnace 
 */
static void
rwtermio_window_resize_handler(rwsched_CFSocketRef s,
		  CFSocketCallBackType type,
		  CFDataRef address,
		  const void *data,
		  void *info)
{
  UNUSED (s);
  UNUSED (type);
  UNUSED (address);
  UNUSED (data);
  char buf[RWTERMIO_READ_SIZE];
  rwtermio_instance_ptr_t instance = (rwtermio_instance_ptr_t)info;

  read(sigwinch_pipe[0], buf, RWTERMIO_READ_SIZE);

  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance, 
      RWTRACE_CATEGORY_RWTASKLET,
      "Terminal Window Resize. Passthrough.");

  // Sync the window size with the pseudo terminal
  rwtermio_sync_window_size(instance);
}

/*
 * Sets the rwmain terminal in RAW mode (almost). All keystrokes are captured
 * and sent to the Pseudo terminal as it is without any formatting or delay.
 * However output formatting is still enabled for the trace logs to be formatted
 * correctly. 
 * @param instance  - Terminal IO takslet instance pointer
 * @returns RW_STATUS_SUCCESS on succesuful operation, RW_STATUS_FAILURE
 *          otherwise
 */
static rw_status_t 
rwtermio_set_terminal_raw(rwtermio_instance_ptr_t instance)
{
  struct termios new_term_settings;
  int ret = 0;

  if (!isatty(STDIN_FILENO)) {
    /* Stdin not a TTY, return failure. This might happen when riftware run
     * without a terminal and RW.CLI is part of the manifest. */
    return RW_STATUS_FAILURE;
  }

  ret = tcgetattr(STDIN_FILENO, &(instance->orig_term_settings));
  if (ret != 0) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to get Terminal settings: %s", strerror(err));
    return RW_STATUS_FAILURE;
  }

  /*
   * Setting the terminal as RAW will disable the line discipline and send
   * character by character without any formatting. NL to CRLF conversion will
   * also be disabled for the input.
   */
  new_term_settings = instance->orig_term_settings;

  new_term_settings.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
      | INLCR | IGNCR | ICRNL | IXON);
  new_term_settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  new_term_settings.c_cflag &= ~(CSIZE | PARENB);
  new_term_settings.c_cflag |= CS8;  

  ret = tcsetattr(STDIN_FILENO, TCSANOW, &new_term_settings);
  if (ret != 0) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to set Terminal settings: %s", strerror(err));
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
  
}

/*
 * Revert the terminal to a sane setting. Cannonical mode is enabled.
 *
 * @param instance  - Terminal IO takslet instance pointer
 * @returns RW_STATUS_SUCCESS on succesuful operation, RW_STATUS_FAILURE
 *          otherwise
 */
static rw_status_t
rwtermio_set_tty_sane(rwtermio_instance_ptr_t instance)
{
  int ret = -1;

  if (!isatty(STDIN_FILENO)) {
    /* Stdin not a TTY, return failure. This might happen when riftware run
     * without a terminal and RW.CLI is part of the manifest. */
    return RW_STATUS_FAILURE;
  }


  ret = tcsetattr(STDIN_FILENO, TCSANOW, &(instance->orig_term_settings));
  if (ret != 0) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to set Terminal settings: %s", strerror(err));
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

/*
 * Creates a pseudo terminal and enable slave handle. Slave descriptor will be
 * opened by the Native process that will be using the pseudo terminal. 
 *
 * @param instance  - Terminal IO takslet instance pointer
 * @returns RW_STATUS_SUCCESS on succesuful operation, RW_STATUS_FAILURE
 *          otherwise
 */
static rw_status_t
rwtermio_open_pty(rwtermio_instance_ptr_t instance) 
{
  int ret = 0;

  /* Open a pseudo terminal and get the master_fd */
  ret = posix_openpt(O_RDWR);
  if (ret == -1) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to open Pseudo Terminal: %s", strerror(err));
    return RW_STATUS_FAILURE;
  }
  instance->master_fd = ret;

  /* Change the mode and ownership of the slave to reflect the master */
  ret = grantpt(instance->master_fd);
  if (ret != 0) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to change ownership of Pseudo terminal: %s", strerror(err));
    return RW_STATUS_FAILURE;
  }

  /* Unlock the slave pseudo terminal for usage(should be called before slave
   * open */
  ret = unlockpt(instance->master_fd);
  if (ret != 0) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to unlock Pseudo terminal: %s", strerror(err));
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

/*
 * Callback received when there is an event on the stdin of rwmain terminal.
 * Input is read and passed to the pseudo terminal. Special handling is done for
 * Ctrl-C.
 * @param info  - TerminalIO tasklet instance
 */
static void 
rwtermio_handle_stdin(rwsched_CFSocketRef s,
		  CFSocketCallBackType type,
		  CFDataRef address,
		  const void *data,
		  void *info)
{
  UNUSED (s);
  UNUSED (type);
  UNUSED (address);
  UNUSED (data);
  char buf[RWTERMIO_READ_SIZE];
  rwtermio_instance_ptr_t instance = (rwtermio_instance_ptr_t)info;

  size_t ret = read(STDIN_FILENO, buf, RWTERMIO_READ_SIZE);
  if (ret < 0) {
    int err = errno;
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      // This callback will be invoked again
      return;
    }
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Error reading from stdin: %s", strerror(err));
    return;
  }

  if (ret == 0) {
    // EOF. Close the socket
    rwsched_tasklet_CFSocketRelease(instance->rwtasklet_info->rwsched_tasklet_info,s);
    return;
  }

  // Since the stdin is operating on a raw mode, special handling is required
  // for the Ctrl-C if under gdb
  if (buf[0] == (char)CTRL_C_CHAR && instance->under_gdb) {
    // Interrupt self - will be trapped by GDB
    kill(getpid(), SIGINT);
    return;
  }

  ret = write(instance->master_fd, buf, ret);
  if (ret < 0) {
    int err = errno;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Error writing to pty: %s", strerror(err));
  }
}

/*
 * Handles the event received on the Pseudo Terminal handle. The data received
 * is written to the STDOUT
 * @param info  - TerminalIO Tasklet instance
 */
static void
rwtermio_handle_pty_out(rwsched_CFSocketRef s,
		  CFSocketCallBackType type,
		  CFDataRef address,
		  const void *data,
		  void *info)
{
  char buf[RWTERMIO_WRITE_SIZE];
  UNUSED (s);
  UNUSED (type);
  UNUSED (address);
  UNUSED (data);
  rwtermio_instance_ptr_t instance = (rwtermio_instance_ptr_t)info;

  size_t ret = read(instance->master_fd, buf, RWTERMIO_WRITE_SIZE);
  if (ret < 0) {
    int err = errno;
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      // This callback will be invoked again
      return;
    }
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Error reading from pseudo terminal: %s", strerror(err));
    return;
  }

  if (ret == 0) {
    // EOF
    RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "EOF received on pseudo terminal");
    rwsched_tasklet_CFSocketRelease(instance->rwtasklet_info->rwsched_tasklet_info,s);
    return;
  }

  // Display it on the stdout
  write(STDOUT_FILENO, buf, ret);
}

/*
 * Handles the inotify event received on slave pseudo terminal 
 * @param info  - TerminalIO Tasklet instance
 */
static void
rwtermio_handle_slave_inotify(rwsched_CFSocketRef s,
		  CFSocketCallBackType type,
		  CFDataRef address,
		  const void *data,
		  void *info)
{
  const int buf_size = sizeof(struct inotify_event) + NAME_MAX + 1;
  char buf[buf_size];
  ssize_t nread = 0;
  struct inotify_event* event;
  UNUSED (s);
  UNUSED (type);
  UNUSED (address);
  UNUSED (data);

  rwtermio_instance_ptr_t instance = (rwtermio_instance_ptr_t)info;

  nread = read(instance->slave_inotify, buf, buf_size);
  if (nread == 0) {
    RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "EOF received on inotify fd");
    rwsched_tasklet_CFSocketRelease(instance->rwtasklet_info->rwsched_tasklet_info,s);
    return;
  } else if (nread == -1) {
    RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance, 
        RWTRACE_CATEGORY_RWTASKLET,
        "Inotify event read failed");
    rwsched_tasklet_CFSocketRelease(instance->rwtasklet_info->rwsched_tasklet_info,s);
    return;
  }

  RW_ASSERT(nread >= sizeof(struct inotify_event));

  event = (struct inotify_event*)buf;
  if (event->mask & IN_OPEN) {
    // nothing todo, not handled currently
    RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
        RWTRACE_CATEGORY_RWTASKLET,
        "Slave Terminal Open event received");
  }

  if ((event->mask & IN_CLOSE_WRITE) || (event->mask & IN_CLOSE_NOWRITE)) {
    RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
        RWTRACE_CATEGORY_RWTASKLET,
        "Slave Terminal close event received");
    
    // Set the terminal as sane
    rwtermio_set_tty_sane(instance);

    // When an interactive process that was launched part of riftware exits it
    // can be assumed that the intention is to terminate the riftware process.
    // Hence terminate the rwmain
    kill(getpid(), SIGTERM);
  }
}

/*
 * Register the socket/file with rwsched_CFRunLoop to handle read events
 * @param instance  - TerminalIO tasklet instance
 * @param sock_fd   - socket/file descriptor on which read event is to be handled
 * @param cbk       - callback method that should be invoked on read event
 */
static void 
register_socket_for_read(rwtermio_instance_ptr_t instance,
      int sock_fd, 
      rwsched_CFSocketCallBack cbk)
{
  CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
  CFOptionFlags cf_callback_flags = 0;
  CFOptionFlags cf_option_flags = 0;
  rwsched_CFRunLoopRef runloop;
  rwsched_CFSocketRef cfsocket;
  rwsched_CFRunLoopSourceRef cfsource;

  // Use the pollbits to determine which socket callback events to register
  cf_callback_flags |= kCFSocketReadCallBack;
  cf_option_flags |= kCFSocketAutomaticallyReenableReadCallBack;

  runloop = rwsched_tasklet_CFRunLoopGetCurrent(
                instance->rwtasklet_info->rwsched_tasklet_info);

  cf_context.info = instance;

  cfsocket = rwsched_tasklet_CFSocketCreateWithNative(
                instance->rwtasklet_info->rwsched_tasklet_info,
					      kCFAllocatorSystemDefault,
					      sock_fd,
					      cf_callback_flags,
					      cbk,
					      &cf_context);
  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);
  
  rwsched_tasklet_CFSocketSetSocketFlags(
                instance->rwtasklet_info->rwsched_tasklet_info, 
                cfsocket, cf_option_flags);

  cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(
                instance->rwtasklet_info->rwsched_tasklet_info,
						    kCFAllocatorSystemDefault,
						    cfsocket,
						    0);
  RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);

  rwsched_tasklet_CFRunLoopAddSource(
                instance->rwtasklet_info->rwsched_tasklet_info,
                runloop,
                cfsource,
                instance->rwtasklet_info->rwsched_instance->main_cfrunloop_mode);
}

/*
 * Timer callback to register the tasklet with Zookeeper. The Zookeper may not
 * be up when the TerminalIO tasklet is started, hence delaying the
 * registration.
 * @param timer - Timer reference
 * @param info  - TerminalIO tasklet instance
 *
 */
static void
rwtermio_cftimer_callback(rwsched_CFRunLoopTimerRef timer, void *info)
{
  rwtasklet_info_ptr_t ti = NULL;
  rwtermio_instance_ptr_t instance = (rwtermio_instance_ptr_t)info;

  ti = instance->rwtasklet_info;
  if (ti->rwvcs) {
    // Register with the zookeeper and update the state to running
    rw_status_t status = RW_STATUS_SUCCESS;
    char* instance_name = to_instance_name(ti->identity.rwtasklet_name,
                                      ti->identity.rwtasklet_instance_id);;
    status = rwvcs_rwzk_update_state(ti->rwvcs, 
                instance_name, 
                RW_BASE_STATE_TYPE_RUNNING);

    RW_ASSERT(RW_STATUS_SUCCESS == status);
    free(instance_name);

    RWTRACE_INFO(ti->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
        "RW.TerminalIO -- Tasklet registered with rwzk!");
  }

}

/*
 * Stars a rwsched timer to register this tasklet with zookeeper.
 * @param instance  - TerminalIO Tasklet instance
 */
static void
start_rwsched_timer(rwtermio_instance_ptr_t instance)
{
  // Create a CFRunLoopTimer and add the timer to the current runloop
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  double timer_interval = 1000 * .001;
  rwsched_CFRunLoopTimerRef cftimer;
  rwsched_CFRunLoopRef runloop;

  runloop = rwsched_tasklet_CFRunLoopGetCurrent(
                instance->rwtasklet_info->rwsched_tasklet_info);
  cf_context.info = instance;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
                instance->rwtasklet_info->rwsched_tasklet_info,
                kCFAllocatorDefault,
                CFAbsoluteTimeGetCurrent() + timer_interval,
                0,
                0,
                0,
                rwtermio_cftimer_callback,
                &cf_context);

  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);

  rwsched_tasklet_CFRunLoopAddTimer(
                instance->rwtasklet_info->rwsched_tasklet_info,
                runloop, 
                cftimer, 
                instance->rwtasklet_info->rwsched_instance->main_cfrunloop_mode);
  
}

/*
 * Add the slave terminal to an inotify watch. If a close event is received on
 * the salve terminal, that means the TerminalIO user (CLI proc) has exited.
 */
static rw_status_t
rwtermio_add_slave_watch(rwtermio_instance_ptr_t instance)
{
  const int term_name_size = 256;
  const uint32_t mask = IN_OPEN | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE;
  char term_name[term_name_size];
  int ret = 0;

  // Get the slave terminal and create an inotify descriptor to watch the slave
  // terminal
  ret = ptsname_r(instance->master_fd, term_name, term_name_size);
  if (ret != 0) {
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance,
        RWTRACE_CATEGORY_RWTASKLET,
        "Failed to obtain Pseudo Terminal Name: %s", 
        strerror(errno));
    return RW_STATUS_FAILURE;
  }

  instance->slave_inotify = inotify_init1(IN_CLOEXEC);
  if (instance->slave_inotify == -1) {
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance,
        RWTRACE_CATEGORY_RWTASKLET,
        "Create Inotify for slave %s failed: %s",
        term_name, strerror(errno));
    return RW_STATUS_FAILURE;
  }

  instance->slave_watch_handle = inotify_add_watch(instance->slave_inotify,
                                    term_name, mask);
  if (instance->slave_watch_handle == -1) {
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance,
        RWTRACE_CATEGORY_RWTASKLET,
        "Create watch for slave %s failed: %s",
        term_name, strerror(errno));
    return RW_STATUS_FAILURE;
  }

  // Now register this FD with the CFRunLoop for events
  register_socket_for_read(instance, instance->slave_inotify,
                           rwtermio_handle_slave_inotify);

  return RW_STATUS_SUCCESS;
}

/*
 * Tasklet Component init method. Allocate memory for the component
 */
rwtermio_component_ptr_t rwtermio_component_init(void) 
{
  rwtermio_component_ptr_t component = NULL;

  // Allocate a new rwtermio_component structure
  component = RW_CF_TYPE_MALLOC0(sizeof(*component), rwtermio_component_ptr_t);
  RW_CF_TYPE_VALIDATE(component, rwtermio_component_ptr_t);

  return component;
}

/*
 * Tasklet component deinit method.
 */
void rwtermio_component_deinit(rwtermio_component_ptr_t component)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwtermio_component_ptr_t);
  return;
}

/*
 * TerminalIO tasklet instance creation (constructor)
 * @param component       - Tasklet component to be instantiated
 * @param rwtasklet_info  - Tasklet information
 * @param instance_url    
 * @returns An instance to TerminalIO tasklet
 */
rwtermio_instance_ptr_t rwtermio_instance_alloc(
        rwtermio_component_ptr_t component, 
        struct rwtasklet_info_s *rwtasklet_info, 
        RwTaskletPlugin_RWExecURL *instance_url)
{
  rwtermio_instance_ptr_t instance = NULL;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwtermio_component_ptr_t);

  // Allocate a new rwcli_instance structure
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwtermio_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwtermio_instance_ptr_t);
  instance->component = component;

  // Save the rwtasklet_info structure
  instance->rwtasklet_info = rwtasklet_info;

  // Return the allocated instance
  return instance;
}

/*
 * Destroys the TerminalIO tasklet instance (desctrutor)
 * @param component - Tasklet component to be destroyed
 * @param instance  - Tasklet instance to be destroyed
 */
void rwtermio_instance_free(
        rwtermio_component_ptr_t component, 
        rwtermio_instance_ptr_t instance)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwtermio_component_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwtermio_instance_ptr_t);

  rwtermio_set_tty_sane(instance);

  return;
}

/*
 * Start the Tasklet. Registers for read events on STDIN, pseudo terminal
 * handle and the signal handler self-pipe.
 * @param component - Tasklet component to be started
 * @param instance  - Tasklet instance to be started
 *
 */
void rwtermio_instance_start(
        rwtermio_component_ptr_t component, 
        rwtermio_instance_ptr_t instance)
{
  int ret = 0;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwtermio_component_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwtermio_instance_ptr_t);
    
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	             RWTRACE_CATEGORY_RWTASKLET,
	             "RW.TerminalIO -- Tasklet is started!");

  instance->under_gdb = is_gdb_attached();

  // Create pseudo terminal
  rwtermio_open_pty(instance);

  // Add a watch on the slave file, to receive slave close event
  rwtermio_add_slave_watch(instance);
  
  // Register for events from stdin only if stdin is a terminal
  if (isatty(STDIN_FILENO)) {
    register_socket_for_read(instance, STDIN_FILENO, rwtermio_handle_stdin);
  }

  // Register for events from master-fd of the pseudo terminal
  register_socket_for_read(instance, instance->master_fd, rwtermio_handle_pty_out);

  rwtermio_set_terminal_raw(instance);
  rwtermio_sync_window_size(instance);

  start_rwsched_timer(instance);

  // Create the sigwinch_pipe to handle the signal in the sched callback instead
  // of the signal handler
  ret = pipe(sigwinch_pipe);
  if (ret != 0) {
    int err = errno;
    RWTRACE_WARN(instance->rwtasklet_info->rwtrace_instance,
	             RWTRACE_CATEGORY_RWTASKLET,
	             "Error creating signwinch pipe: %s", strerror(err));
  }

  register_socket_for_read(instance, sigwinch_pipe[0], 
              rwtermio_window_resize_handler);

  // Register for SIGWINCH signal, to get window resize event
  rwsched_tasklet_signal(instance->rwtasklet_info->rwsched_tasklet_info, 
              SIGWINCH, rwtermio_signwinch_handler, NULL, NULL);

  return;
}

