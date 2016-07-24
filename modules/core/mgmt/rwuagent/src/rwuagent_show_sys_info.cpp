
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwuagent_show_sys_info.cpp
 * @date 2015/11/11
 * Management agent show system info command.
 */

#include "rwuagent.hpp"

#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

namespace fs = boost::filesystem;
using namespace rw_uagent;
using namespace rw_yang;

// ATTN: better place for this
#define RW_MGMT_AGENT_SSD_PATH "/usr/rift/cli/scripts/rw_ssd.zsh"
#define RW_MGMT_AGENT_ZSH_PATH "/usr/bin/zsh"

#define CHILD_STATUS_POLL_TIMER 1 // In secs.
#define SSI_READ_BUFFER 24*1024 // 24K Should depend on OS PIPESZ?

ShowSysInfo::ShowSysInfo(
    Instance* instance,
    SbReqRpc* parent_rpc,
    const RWPB_T_MSG(RwMgmtagt_input_ShowSystemInfo)* sysinfo_in )
: instance_(instance),
  memlog_buf_(
    instance->get_memlog_inst(),
    "ShowSysInfo",
    reinterpret_cast<intptr_t>(this)),
  parent_rpc_(parent_rpc),
  sysinfo_in_(sysinfo_in)
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "show system info",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)parent_rpc) );

  if (sysinfo_in->file) {
    ssi_fname_ = sysinfo_in->file;
  } else {
    ssi_as_string_ = true;
  }
}

ShowSysInfo::~ShowSysInfo()
{
  if (output_buff_ != nullptr) {
    RW_FREE(output_buff_);
    output_buff_ = nullptr;
  }
}

NetconfError& ShowSysInfo::add_error(
  RwNetconf_ErrorType errType,
  IetfNetconf_ErrorTagType errTag )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "add error",
    RWMEMLOG_ARG_PRINTF_INTPTR("type=%" PRIuPTR, (intptr_t)errType),
    RWMEMLOG_ARG_PRINTF_INTPTR("tag=%" PRIuPTR, (intptr_t)errTag) );
  return nc_errors_.add_error( errType, errTag );
}

StartStatus ShowSysInfo::respond_with_error()
{
  parent_rpc_->internal_done(&nc_errors_);
  delete this; // Delete me.
  return StartStatus::Done;
}

StartStatus ShowSysInfo::execute()
{
#define RETURN_IOERR(cond_, sys_call_) \
    if (!(cond_)) { \
      RW_MA_INST_LOG(instance_, InstanceError, sys_call_); \
      add_error().set_errno(sys_call_); \
      return respond_with_error(); \
    }

  if (ssi_as_string_) {

    // Need a pipe to get output fromchild to parent
    int fds[2];
    auto ioerr = pipe(fds);
    RETURN_IOERR( ioerr == 0, "pipe failed" );

    parent_read_fd_ = fds[0];
    child_write_fd_ = fds[1];

    // Make the read-end non-blocking
    int flags = fcntl(parent_read_fd_, F_GETFL, 0);
    RETURN_IOERR( flags >= 0, "fcntl failed" );

    ioerr = fcntl(parent_read_fd_, F_SETFL, flags | (int) O_NONBLOCK);
    RETURN_IOERR( ioerr >=0, "fcntl failed" );

  } else {

    RW_ASSERT (ssi_fname_.length());

    // Open the file for writing. If exists append, else create
    child_write_fd_ = open(ssi_fname_.c_str(), O_WRONLY | O_APPEND | O_CREAT);
    RETURN_IOERR ( child_write_fd_ > 0, "Failed to open file" );
  }

  // Fork
  child_pid_ = fork();
  switch (child_pid_) {
    case -1:
      RW_MA_INST_LOG(instance_, InstanceError, "fork failed");
      add_error().set_errno( "fork" );
      return respond_with_error();

    case 0:
      // Close the read end in the child.
      if (parent_read_fd_ != -1) {
        close(parent_read_fd_);
        parent_read_fd_ = -1;
      }
      execute_child_process();
      RW_ASSERT_NOT_REACHED();

    default:
      break;
  }

  // Close the write end of the fd.
  close(child_write_fd_);
  child_write_fd_ = -1;

  auto tasklet = instance_->rwsched_tasklet();
  if (parent_read_fd_ != -1) {

    // Create dispatch source to read the data
    child_read_src_ = rwsched_dispatch_source_create(
        tasklet, RWSCHED_DISPATCH_SOURCE_TYPE_READ, parent_read_fd_,
        0/*mask*/,
        instance_->get_queue(QueueType::DefaultSerial));

    rwsched_dispatch_set_context( tasklet, child_read_src_, this );
    rwsched_dispatch_source_set_event_handler_f(
        tasklet, child_read_src_, &ShowSysInfo::pipe_ready );
    rwsched_dispatch_resume( tasklet, child_read_src_ );
  }

  // Create a timer source to poll for SIGCHILD.
  // The rwsched support for signal handling cannot be used here,
  // because we want the callback to be in queue-context.
  // The libdispatch signal source also doesn't seem to work.
  child_status_poll_timer_  = rwsched_dispatch_source_create(
      tasklet, RWSCHED_DISPATCH_SOURCE_TYPE_TIMER, 0, 0, instance_->get_queue(QueueType::DefaultSerial));

  rwsched_dispatch_source_set_event_handler_f(
      tasklet, child_status_poll_timer_, poll_for_child_status);

  rwsched_dispatch_set_context(tasklet, child_status_poll_timer_, this);

  rwsched_dispatch_source_set_timer(
      tasklet, child_status_poll_timer_,
      dispatch_time(DISPATCH_TIME_NOW, 0*NSEC_PER_SEC), /* Start now */
      CHILD_STATUS_POLL_TIMER*NSEC_PER_SEC, /* Check Every 1 sec */
      0);

  rwsched_dispatch_resume(tasklet, child_status_poll_timer_);
  return StartStatus::Async;

#undef RETURN_IOERR
}

void ShowSysInfo::execute_child_process()
{
#define ASSERT_IOERR(cond_, ...) \
    if (!(cond_)) { \
      std::string lstr; \
      RW_MA_INST_LOG(instance_, InstanceError, "Failed to spawn RW.MgmtAgent CLI child process"); \
      RW_MA_INST_LOG(instance_, InstanceError, __VA_ARGS__); \
      RW_MA_INST_LOG(instance_, InstanceError, (lstr="errno: %s" + std::to_string(errno)).c_str()); \
      abort(); \
    }

  // If my parent dies, I die, too.
  int ioerr = prctl(PR_SET_PDEATHSIG, SIGKILL);
  ASSERT_IOERR( ioerr == 0, "bad prctrl" );

  // Send my output back to the parent or write to the file
  ioerr = dup2( child_write_fd_, STDOUT_FILENO );
  ASSERT_IOERR( ioerr == STDOUT_FILENO, "bad dup stdout" );
  // ATTN: Leaving STDERR alone is correct?

  // Set input to /dev/null.
  int devnull_fd = open( "/dev/null", O_RDONLY );
  ASSERT_IOERR( ioerr >= 0, "could not open /dev/null" );
  ioerr = dup2( devnull_fd, STDIN_FILENO );
  ASSERT_IOERR( ioerr == STDIN_FILENO, "bad dup stdin" );

  // ATTN: Leaving STDERR alone is correct?

  // Close all other FDs
  struct rlimit rl = {0};
  ioerr = getrlimit( RLIMIT_NOFILE, &rl );
  ASSERT_IOERR( ioerr == 0, "bad getrlimit" );
  for (int fd = 3; fd < (int)rl.rlim_cur; fd++) {
    close(fd);
  }

  // Now invoke the cli
  auto rift_install = get_rift_install();

  char* ssd_path = nullptr;
  ioerr = asprintf( &ssd_path, "%s%s", rift_install.c_str(), RW_MGMT_AGENT_SSD_PATH );
  ASSERT_IOERR( ioerr > 0, "bad ssd path" );

  char* zsh_path = nullptr;
  ioerr = asprintf( &zsh_path, "%s%s", rift_install.c_str(), RW_MGMT_AGENT_ZSH_PATH );
  ASSERT_IOERR( ioerr > 0, "bad zsh path" );

  // Get my VM INSTANCE ID. ATTN: Change this to use zk api
  uint32_t instance_id = instance_->rwvcs()->identity.rwvm_instance_id;
  RW_ASSERT (instance_id);

  std::vector<const char*> argv;
  argv.emplace_back( zsh_path );
  argv.emplace_back( "--rwmsg" );
  argv.emplace_back( "--username");
  argv.emplace_back( "admin");
  argv.emplace_back( "--passwd");
  argv.emplace_back( "admin");
  argv.emplace_back( ssd_path );
  argv.emplace_back( nullptr );

  std::stringstream ss;
  ss << "Invoking the zsh CLI cmd = ";
  std::for_each(argv.begin(), argv.end(), [&ss](const char*arg) { ss << " " << arg;} );

  std::string capture_temporary;
  RW_MA_INST_LOG(instance_, InstanceInfo, (capture_temporary=ss.str()).c_str());

  ioerr = execv( argv[0], (char* const*)argv.data() );
  ASSERT_IOERR( 0, "failed exec" );
  RW_ASSERT_NOT_REACHED();

#undef ASSERT_IOERR
}

void ShowSysInfo::cleanup_and_send_response()
{
  if (parent_read_fd_ != -1) {
    close(parent_read_fd_);
    parent_read_fd_ = -1;
  }

  auto tasklet = instance_->rwsched_tasklet();
  if (child_read_src_ != NULL) {
    rwsched_dispatch_source_cancel(tasklet, child_read_src_);
    rwsched_dispatch_release(tasklet, child_read_src_);
    child_read_src_ = NULL;
  }

  RWPB_M_MSG_DECL_INIT(RwMgmtagt_SysInfoOutput, rpco);

  if (ssi_as_string_) {
    if (rsp_bytes_ > 0) {

      RW_ASSERT(output_buff_);
      RW_ASSERT(rsp_bytes_ < curr_buff_sz_);
      output_buff_[rsp_bytes_] = 0; // Null terminate the string.

      // Create the output ks and msg.
      // send response to the parent rpc.
      rpco.result = output_buff_;

      std::string log_string;
      RW_MA_INST_LOG(instance_, InstanceInfo,
                     (log_string = "SSI, Sending total bytes " + std::to_string(rsp_bytes_)).c_str());

    } else {
      if (!nc_errors_.length()) {
        add_error().set_error_message("Failed to retrive SSI");
      }
    }
  } else {

    RW_ASSERT (ssi_fname_.length());
    RW_ASSERT (!output_buff_);

    // make ssi file readable
    fs::permissions(ssi_fname_,
                    fs::owner_read | fs::owner_write |
                    fs::group_read | fs::group_write |
                    fs::others_read);

    if (!nc_errors_.length()) {
      rpco.result = output_buff_ = strdup("SSI Saved to file");
    }
  }

  if (rpco.result) {
    parent_rpc_->internal_done(
        &RWPB_G_PATHSPEC_VALUE(RwMgmtagt_SysInfoOutput)->rw_keyspec_path_t, &rpco.base);
    // ATTN:- When the above call completes, we should have sent all the data.
  } else {

    RW_ASSERT(nc_errors_.length());
    parent_rpc_->internal_done(&nc_errors_);
  }

  responded_ = true;
  parent_rpc_ = NULL; // Parent rpc might have been destroyed. Invalidate it for safety.

  /*
   * Deletion of this object should happen only after successful reaping of the
   * child process.
   */
}


void ShowSysInfo::read_from_pipe()
{
  int rc = 0;

  if (output_buff_ == nullptr) {
    output_buff_ = (char *)RW_MALLOC(SSI_READ_BUFFER);
    RW_ASSERT (output_buff_);
    curr_buff_sz_ = SSI_READ_BUFFER;
  }

  // Read the pipe
  while (1) {

    size_t read_sz = curr_buff_sz_ - rsp_bytes_;

    if (read_sz < 2) { // ATTN: Is this ok?
      output_buff_ = (char *)RW_REALLOC(output_buff_, curr_buff_sz_+SSI_READ_BUFFER);
      RW_ASSERT(output_buff_);
      curr_buff_sz_ += SSI_READ_BUFFER;

      read_sz = curr_buff_sz_ - rsp_bytes_;
    }

    RW_ASSERT(read_sz);
    rc = read(parent_read_fd_, output_buff_+rsp_bytes_, read_sz - 1);

    if (rc > 0) {
      rsp_bytes_ += rc;
      continue;
    }

    if (rc < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return;
      }
      add_error().set_errno("read");
      RW_MA_INST_LOG (instance_, InstanceError, "SSI, Failed to read from the pipe");
    }

    if (rc == 0) {
      RW_MA_INST_LOG (instance_, InstanceInfo, "SSI, remote end of the pipe closed");
    }

    break;
  }

  cleanup_and_send_response();
}

void ShowSysInfo::pipe_ready(
    void* ctx )
{
  ShowSysInfo *ssi = static_cast<ShowSysInfo *>(ctx);
  RW_ASSERT(ssi);
  ssi->read_from_pipe();
}

void ShowSysInfo::poll_for_child_status(void *ctx)
{
  ShowSysInfo *ssi = static_cast<ShowSysInfo*>(ctx);
  RW_ASSERT (ssi);
  ssi->try_reap_child();
}

void ShowSysInfo::async_self_delete(void *ctx)
{
  ShowSysInfo *ssi = static_cast<ShowSysInfo*>(ctx);
  RW_ASSERT (ssi);
  if (!ssi->get_responded()) {
    ssi->cleanup_and_send_response();
  }
  delete ssi;
}

void ShowSysInfo::try_reap_child()
{
  int child_status = 0;
  pid_t pid = waitpid( child_pid_, &child_status, WNOHANG );

  if (0 == pid) {
    // Child is still running.. Continue to poll..
    return;
  }

  if (pid == child_pid_) {
    // The child exited.

    // Cancel the timer source.
    auto tasklet = instance_->rwsched_tasklet();
    rwsched_dispatch_source_cancel(tasklet, child_status_poll_timer_);
    child_status_poll_timer_ = NULL;

    if (!WIFEXITED(child_status)) {
      // Abnormal exit..
      add_error().set_error_message("CLI execution failed");
      RW_MA_INST_LOG(instance_, InstanceError, "SSI, CLI process execution failed");
    } else {
      auto exit_status = WEXITSTATUS(child_status);

      std::string log_string;
      RW_MA_INST_LOG(instance_, InstanceDebug,
                     (log_string = "SSI, CLI exited with status = " + std::to_string(exit_status)).c_str());

      if (exit_status != 0) {
        add_error().set_error_message("Non zero exit status.Failed to execute SSI");
      }
    }

    if (ssi_as_string_) {
      // Delete myself asynchronously to avoid possible race conditions.
      auto when = dispatch_time(DISPATCH_TIME_NOW, 0.01 * NSEC_PER_SEC); // after 100 ms. ok?
      rwsched_dispatch_after_f(tasklet,
                               when,
                               instance_->get_queue(QueueType::DefaultSerial),
                               this,
                               async_self_delete);
    } else {
      // Not required to delay, when save to file is enabled.
      async_self_delete(this);
    }
    return;
  }

  //ATTN: Error on waitpid. What to do here??
}
