
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file   rw_ut_confd.cpp
 * @author Tom Seidenberg
 * @date   2014/08/14
 * @brief  RW unit test confd spawner
 */

#include "rw_ut_confd.hpp"

#include <confd_lib.h>
#include <confd_cdb.h>
#include <confd_dp.h>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <iostream>

using namespace rw_yang;
namespace FS = boost::filesystem;
using std::cerr;
using std::cout;

ConfdUnittestHarness::ConfdUnittestHarness(
  const char* test_name)
: test_name_(test_name)
{
  // Find RIFT_ROOT
  const char* rift_root = getenv("RIFT_ROOT");
  if (nullptr == rift_root) {
    cerr << "Unable to find $RIFT_ROOT" << std::endl;
    throw std::exception();
  }
  rift_root_ = rift_root;

  // Verify confd root exists
  confd_install_root_ = rift_root_/".install/usr/local/confd";
  if (!exists(confd_install_root_)) {
    cerr << "Unable to find confd root " << confd_install_root_ << std::endl;
    throw std::exception();
  }
  if (!is_directory(confd_install_root_)) {
    cerr << "Supposed confd root is not a directory " << confd_install_root_ << std::endl;
    throw std::exception();
  }

  cwd_ = FS::current_path();

  // Determine the test root directory.
  test_root_ = rift_root_/".install/tmp/confd"/test_name_;
  if (exists(test_root_) && !is_directory(test_root_)) {
    cerr << "Supposed test root exists, but is not directory " << confd_install_root_ << std::endl;
    throw std::exception();
  }

  // Create socket path and verify that it is not too long.
  char user[L_cuserid+1] = "unknown";
  getlogin_r(user, sizeof(user) );
  socket_path_ = FS::path("/tmp") / (std::string("confd_ut_")+user) / test_name_ / std::to_string(getpid());
  RW_ASSERT(socket_path_.native().length() < sizeof(sockaddr_un::sun_path));

  confd_conf_path_ = test_root_/"etc/confd.conf";

  // Create a DOM for holding confd.conf while it is being configured.
  xml_mgr_ = xml_manager_create_xerces();
  confd_conf_dom_ = xml_mgr_->create_document("confdConfig", "http://tail-f.com/ns/confd_cfg/1.0");
}

ConfdUnittestHarness::~ConfdUnittestHarness()
{
  stop();
}

ConfdUnittestHarness::uptr_t ConfdUnittestHarness::create_harness_manual(
  const char* test_name)
{
  RW_ASSERT(test_name);

  // Create the harness.
  uptr_t harness(new ConfdUnittestHarness(test_name));

  // Return the harness to the caller
  return harness;
}

ConfdUnittestHarness::uptr_t ConfdUnittestHarness::create_harness_autostart(
  const char* test_name,
  YangModel* yang_model)
{
  RW_ASSERT(test_name);
  RW_ASSERT(yang_model);

  // Create the harness.
  uptr_t harness(new ConfdUnittestHarness(test_name));

  // Load the default configuration DOM.
  harness->conf_load_default();

  // Initialize the DOM Paths.
  harness->conf_init_paths();

  // Populate the directory structure first, because loading the schema
  // requires the test root to exist
  harness->populate_test_dir();

  // Now load the schema, which may populate more fxs files
  harness->load_fxs_schema(yang_model);

  // Start the confd server
  harness->start();

  // Return the harness to the caller
  return harness;
}

void ConfdUnittestHarness::conf_load_default()
{
  conf_load(confd_install_root_/"etc/confd/confd.conf.harness");
}

void ConfdUnittestHarness::conf_load(
  FS::path filename)
{
  try {
    if (!filename.is_absolute()) {
      filename = cwd_/filename;
    }

    if (!exists(filename)) {
      cerr << "Could not find harness configuration " << filename << std::endl;
      throw std::exception();
    }

    confd_conf_dom_ = std::move(xml_mgr_->create_document_from_file(filename.c_str(), false/*validate*/));
    if (!confd_conf_dom_.get()) {
      cerr << "Failed to load configuration " << filename << std::endl;
      throw std::exception();
    }

  } catch (const FS::filesystem_error& fe) {
    cerr << "Filesystem error: " << fe.what()
         << ": " << fe.path1()
         << ", " << fe.path2()
         << std::endl;
    throw std::exception(); // don't rethrow, to prevent being recaught by harness
  }
}

void ConfdUnittestHarness::conf_init_paths()
{
  XMLNode* root_node = confd_conf_dom_->get_root_node();
  RW_ASSERT(root_node);

  XMLNode* loadPath = root_node->find("loadPath");
  if (!loadPath) {
    loadPath = root_node->add_child("loadPath");
    RW_ASSERT(loadPath);
  }
  XMLNode* dir = loadPath->add_child("dir", (test_root_/"etc").c_str());
  RW_ASSERT(dir);

  XMLNode* stateDir = root_node->find("stateDir");
  if (!stateDir) {
    stateDir = root_node->add_child("stateDir", (test_root_/"var/state").c_str());
    RW_ASSERT(stateDir);
  }

  XMLNode* cdb = root_node->find("cdb");
  if (!cdb) {
    cdb = root_node->add_child("cdb");
    RW_ASSERT(cdb);
  }
  XMLNode* dbDir = cdb->find("dbDir");
  if (!dbDir) {
    dbDir = cdb->add_child("dbDir", (test_root_/"var/cdb").c_str());
    RW_ASSERT(dbDir);
  }

  XMLNode* aaa = root_node->find("aaa");
  if (!aaa) {
    aaa = root_node->add_child("aaa");
    RW_ASSERT(aaa);
  }
  XMLNode* sshServerKeyDir = aaa->find("sshServerKeyDir");
  if (!sshServerKeyDir) {
    sshServerKeyDir = aaa->add_child("sshServerKeyDir", (test_root_/"etc/ssh").c_str());
    RW_ASSERT(sshServerKeyDir);
  }

  XMLNode* confdExternalIpc = root_node->find("confdExternalIpc");
  if (!confdExternalIpc) {
    confdExternalIpc = root_node->add_child("confdExternalIpc");
    RW_ASSERT(confdExternalIpc);
  }
  XMLNode* address = confdExternalIpc->find("address");
  if (!address) {
    address = confdExternalIpc->add_child("address", (socket_path_).c_str());
    RW_ASSERT(address);
  }
  XMLNode* enabled = confdExternalIpc->find("enabled");
  if (!enabled) {
    enabled = confdExternalIpc->add_child("enabled");
    RW_ASSERT(enabled);
  }
  enabled->set_text_value("true");

  XMLNode* datastores = root_node->find("datastores");
  if (!datastores) {
    datastores = root_node->add_child("datastores");
    RW_ASSERT(datastores);
  }
  XMLNode* candidate = datastores->find("candidate");
  if (!candidate) {
    candidate = datastores->add_child("candidate");
    RW_ASSERT(candidate);
  }
  XMLNode* filename = candidate->find("filename");
  if (!filename) {
    filename = candidate->add_child("filename", (test_root_/"var/candidate/candidate.db").c_str());
    RW_ASSERT(filename);
  }

  XMLNode* netconf = root_node->find("netconf");
  if (!netconf) {
    netconf = root_node->add_child("netconf");
    RW_ASSERT(netconf);
  }
  XMLNode* capabilities = netconf->find("capabilities");
  if (!capabilities) {
    capabilities = netconf->add_child("capabilities");
    RW_ASSERT(capabilities);
  }
  XMLNode* url = capabilities->find("url");
  if (!url) {
    url = capabilities->add_child("url");
    RW_ASSERT(url);
  }
  XMLNode* file = url->find("file");
  if (!file) {
    file = url->add_child("file");
    RW_ASSERT(file);
  }
  XMLNode* rootDir = file->find("rootDir");
  if (!rootDir) {
    rootDir = file->add_child("rootDir", (test_root_/"var/state").c_str());
    RW_ASSERT(rootDir);
  }
}

XMLDocument* ConfdUnittestHarness::conf_get_doc()
{
  return confd_conf_dom_.get();
}

void ConfdUnittestHarness::populate_test_dir()
{
  try {
    remove_all(test_root_);

    FS::path dir = test_root_/"etc";
    create_directories(dir);

    FS::path srcf = confd_install_root_/"etc/confd/confd.ccl";
    FS::path newf = dir/"confd.ccl";
    create_symlink(srcf, newf);

    dir = test_root_/"etc/ssh";
    create_directory(dir);

    srcf = confd_install_root_/"etc/confd/ssh/ssh_host_dsa_key";
    newf = dir/"ssh_host_dsa_key";
    create_symlink(srcf, newf);

    srcf = confd_install_root_/"etc/confd/ssh/ssh_host_dsa_key.pub";
    newf = dir/"ssh_host_dsa_key.pub";
    create_symlink(srcf, newf);

    dir = test_root_/"var";
    create_directory(dir);

    dir = test_root_/"var/candidate";
    create_directory(dir);

    dir = test_root_/"var/cdb";
    create_directory(dir);

    srcf = confd_install_root_/"var/confd/cdb/aaa_init.xml";
    newf = dir/"aaa_init.xml";
    create_symlink(srcf, newf);

    dir = test_root_/"var/log";
    create_directory(dir);

    dir = test_root_/"var/rollback";
    create_directory(dir);

    dir = test_root_/"var/state";
    create_directory(dir);

    // Link all the .fxs files
    dir = confd_install_root_/"etc/confd";
    link_fxs_files(dir);

  } catch (const FS::filesystem_error& fe) {
    cerr << "Filesystem error: " << fe.what()
         << ": " << fe.path1()
         << ", " << fe.path2()
         << std::endl;
    throw std::exception(); // don't rethrow, to prevent being recaught by harness
  }
}

FS::path ConfdUnittestHarness::make_rooted_path(
  FS::path filename)
{
  try {
    // Can handle $RIFT_ROOT
    std::string raw_fullpath = filename.native();
    const char RIFT_ROOT[] = "$RIFT_ROOT";
    if (raw_fullpath.substr(0, sizeof(RIFT_ROOT)-1) == RIFT_ROOT) {
      raw_fullpath.replace(0, sizeof(RIFT_ROOT)-1, rift_root_.c_str());
    }

    FS::path fullpath = raw_fullpath;
    if (!fullpath.is_absolute()) {
      fullpath = cwd_/fullpath;
    }
    return fullpath;

  } catch (const FS::filesystem_error& fe) {
    cerr << "Filesystem error: " << fe.what()
         << ": " << fe.path1()
         << ", " << fe.path2()
         << std::endl;
    throw std::exception(); // don't rethrow, to prevent being recaught by harness
  }
}

void ConfdUnittestHarness::load_fxs_schema(
  YangModel* yang_model)
{
  try {
    /*
     * Ordered list of search paths for the modules.  This list assumes
     * that either the unittest directory contains all relevant
     * modules, or that the modules are installed in the normal place
     * (or some limited variation thereof).
     */
    FS::path search_paths[] = {
      FS::path{cwd_},
      FS::path{cwd_/"yang"},
      FS::path{rift_root_/".install/usr/data/yang"/test_name_},
      FS::path{rift_root_/".install/usr/data/yang/unittest"},
      FS::path{rift_root_/".install/usr/data/yang"},
    };

    // Assume all explicitly loaded modules are the only ones needed.
    for (auto mi = yang_model->module_begin(); mi != yang_model->module_end(); ++mi) {
      if (mi->was_loaded_explicitly()) {
        FS::path fxsname = mi->get_name();
        fxsname += ".fxs";
        bool found = false;

        for (const auto& path: search_paths) {
          FS::path srcf = path/fxsname;
          if (exists(srcf)) {
            link_one_fxs_file(srcf);
            found = true;
            break;
          }
        }

        if (!found) {
          cerr << "Unable to find fxs file for module " << mi->get_name() << std::endl;
          throw std::exception();
        }
      }
    }

    // Some explicit yang modules needed for testing rw-composite.
    std::vector<FS::path> impl_list;
    impl_list.push_back("rw-yang-types.fxs");
    impl_list.push_back("rw-log-d.fxs");
    impl_list.push_back("rw-3gpp-types.fxs");

    for (const auto& fn:impl_list) {
      bool found = false;
      for (const auto& path: search_paths) {
        FS::path srcf = path/fn;
        if (exists(srcf)) {
          link_one_fxs_file(srcf);
          found = true;
          break;
        }
      }

      if (!found) {
        cerr << "Unable to find fxs file for module " << fn << std::endl;
        throw std::exception();
      }
    }

  } catch (const FS::filesystem_error& fe) {
    cerr << "Filesystem error: " << fe.what()
         << ": " << fe.path1()
         << ", " << fe.path2()
         << std::endl;
    throw std::exception(); // don't rethrow, to prevent being recaught by harness
  }
}

void ConfdUnittestHarness::link_fxs_files(
  const FS::path& srcdir)
{
  try {
    // Link all the .fxs files
    for (FS::directory_iterator di(srcdir); di != FS::directory_iterator(); ++di) {
      const FS::path& srcf = di->path();
      if (srcf.extension() == ".fxs") {
        link_one_fxs_file(srcf);
      }
    }
  } catch (const FS::filesystem_error& fe) {
    cerr << "Filesystem error: " << fe.what()
         << ": " << fe.path1()
         << ", " << fe.path2()
         << std::endl;
    throw std::exception(); // don't rethrow, to prevent being recaught by harness
  }
}

void ConfdUnittestHarness::link_one_fxs_file(
  const FS::path& srcf)
{
  try {
    // Link all the .fxs files
    FS::path newf = test_root_/"etc"/srcf.filename();
    create_symlink(srcf, newf);
  } catch (const FS::filesystem_error& fe) {
    cerr << "Filesystem error: " << fe.what()
         << ": " << fe.path1()
         << ", " << fe.path2()
         << std::endl;
    throw std::exception(); // don't rethrow, to prevent being recaught by harness
  }
}

void ConfdUnittestHarness::start()
{
  rw_status_t rs = confd_conf_dom_->to_file(confd_conf_path_.c_str());
  if (RW_STATUS_SUCCESS != rs) {
    cerr << "Failed to serialize config to " << confd_conf_path_ << std::endl;
    throw std::exception();
  }

  /*
   * Delete the old socket, if any.  It is the confd start sentinel,
   * which this function uses to detect when confd has finished
   * initializing.
   */
  unlink(socket_path_.c_str());

  /* Create the path to the socket */
  FS::path socket_dir = socket_path_;
  socket_dir.remove_filename();
  create_directories(socket_dir);

  int fds[2];
  int st = pipe(fds);
  RW_ASSERT(0 == st);

  int fd_confd_stdin = fds[0];
  confd_fd_ = fds[1];

  st = pipe2(fds, O_CLOEXEC);
  RW_ASSERT(0 == st);
  int fd_exec_errno_read = fds[0];
  int fd_exec_errno_write = fds[1];

  FS::path confd_exe = rift_root_/".install/usr/local/confd/bin/confd";

  const char* argv[] = {
    confd_exe.c_str(),
    "-c",
    confd_conf_path_.c_str(),
    "--foreground",
    "--verbose",
    "--stop-on-eof",
    "--ignore-initial-validation",
    NULL
  };

  const char* envp[] = {
    NULL
  };

  cerr << "Spawning confd:";
  for (const char* arg: argv) {
    if (arg) {
      cerr << " " << arg;
    }
  }
  cerr << std::endl;

  struct sigaction sa = {};
  sa.sa_sigaction = &handle_sig;
  sa.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
  st = sigaction(SIGCHLD, &sa, &old_sa_);
  RW_ASSERT(0 == st);
  has_old_sa_ = true;

  cout.flush();
  cerr.flush();

  confd_pid_ = fork();
  RW_ASSERT(confd_pid_ >= 0);
  if (0 == confd_pid_) {
    // Close the parent's FDs
    close(fd_exec_errno_read);
    close(confd_fd_);

    // Swap STDIN
    close(STDIN_FILENO);
    st = dup2(fd_confd_stdin, STDIN_FILENO);
    RW_ASSERT(0 == st);

    // Close unneeded FDs before exec, so as not to pollute FD space for confd
    for (int fd = STDERR_FILENO+1; fd < 1024; ++fd) {
      if (fd != fd_exec_errno_write) {
        close(fd);
      }
    }

    // Try to exec confd and handle failures
    st = execve(argv[0], const_cast<char**>(argv), const_cast<char**>(envp));
    int e = errno;
    write(fd_exec_errno_write, &e, sizeof(e));
    _exit(1);
  }

  // Close the child's FDs
  close(fd_exec_errno_write);
  close(fd_confd_stdin);

  int e;
  ssize_t bytes = read(fd_exec_errno_read, &e, sizeof(e));
  switch (bytes) {
    case -1:
      // error in read()?
      RW_ASSERT_NOT_REACHED();
    case 0:
      // EOF - succesful exec
      break;
    case sizeof(e):
      // errno - unsuccessful exec
      cerr << "Failed to exec " << confd_conf_path_ << ": " << strerror(e) << std::endl;
      throw std::exception();
    default:
      // non-sensical return value from read()?
      RW_ASSERT_NOT_REACHED();
  }
  close(fd_exec_errno_read);

  /*
   * Setup the test application's confd environment variables to point
   * to the external-IPC socket.
   */
  st = setenv("CONFD_DIR", confd_install_root_.c_str(), 1/*overwrite*/);
  RW_ASSERT(0 == st);
  st = setenv("CONFD_IPC_EXTADDR", socket_path_.c_str(), 1/*overwrite*/);
  RW_ASSERT(0 == st);
  FS::path ipc_drv_ops = confd_install_root_/"lib/confd/lib/core/confd/priv/ipc_drv_ops.so";
  setenv("CONFD_IPC_EXTSOPATH", ipc_drv_ops.c_str(), 1/*overwrite*/);

  /*
   * Poll a little while for confd socket to appear.
   */
  for (unsigned i = 0; i < 8; ++i) {
    if (exists(socket_path_)) {
      cerr << "Found socket on attempt " << i << std::endl;
      break;
    }
    if (i > 3) {
      cerr << "Still waiting for confd to start..." << std::endl;
    }
    sleep(1);
  }
}

void ConfdUnittestHarness::stop()
{
  // Close the fd to confd; this should cause confd to exit
  if (confd_fd_ >= 0) {
    close(confd_fd_);
    confd_fd_ = -1;
  }
  // PID cannot be 0?
  RW_ASSERT(confd_pid_);

  // Cleanup the confd process
  if (confd_pid_ > 0) {
    static const int signals[] = { SIGHUP, SIGTERM, SIGKILL };
    static const int sigcnt = sizeof(signals)/sizeof(signals[0]);
    unsigned alarm_time = 2;

    struct sigaction sa_old = {};
    struct sigaction sa = {};
    sa.sa_sigaction = &handle_sig;
    sa.sa_flags = SA_SIGINFO;
    int st = sigaction(SIGALRM, &sa, &sa_old);
    RW_ASSERT(0 == st);

    for (int sig_index = 0; sig_index <=/*yes*/ sigcnt; ++sig_index ){
      alarm(alarm_time);
      pid_t child = waitpid(confd_pid_, nullptr, 0);
      alarm(0);
      alarm_time = alarm_time * 2 - 1;

      if (child == confd_pid_) {
        confd_pid_ = -1;
        break;
      }

      RW_ASSERT(-1 == child);
      if (sig_index == sigcnt) {
        // Oh well, give up.
        cerr << "confd " << confd_pid_
             << " is more powerful than SIGKILL" << std::endl;
        confd_pid_ = -1;
        break;
      }

      switch (errno) {
        case EINTR:
          cerr << "confd has not yet halted, killing " << confd_pid_
               << " with " << signals[sig_index] << std::endl;
          kill(confd_pid_, signals[sig_index]);
          continue;
        case ECHILD:
          confd_pid_ = -1;
          break;
        default:
          RW_ASSERT_NOT_REACHED();
      }
    }
    st = sigaction(SIGALRM, &sa_old, nullptr);
    RW_ASSERT(0 == st);
  }

  // Free the confd context
  if (confd_context_) {
    confd_release_daemon(confd_context_);
    confd_context_ = nullptr;
  }

  // Restore SIGCHLD handling
  if (has_old_sa_) {
    int st = sigaction(SIGCHLD, &old_sa_, nullptr);
    RW_ASSERT(0 == st);
    has_old_sa_ = false;
  }

  // Remove the test directory tree
  if (!keep_test_dirs_) {
    try {
      remove_all(test_root_);
    } catch (...) {
      // just eat all errors
    }
  }
}

bool ConfdUnittestHarness::is_running()
{
  RW_ASSERT(confd_pid_); // zero indicates a race with start(), and therefore threads, and therefore a usage error

  if (confd_pid_ < 0) {
    return false;
  }

  pid_t child = waitpid(confd_pid_, nullptr, WNOHANG);
  if (child == confd_pid_) {
    confd_pid_ = -1;
    return false;
  }

  return true;
}

bool ConfdUnittestHarness::wait_till_phase2(sockaddr_t* sun)
{
  auto sock = socket(sun->sun_family, SOCK_STREAM, 0);
  RW_ASSERT(sock >= 0);
  
  auto ret = cdb_connect(sock, CDB_READ_SOCKET, (sockaddr*)sun, sizeof(sockaddr_t));
  if (ret != CONFD_OK) {
    std::cerr << "cdb_connect failed: " << std::string(confd_lasterr()) << std::endl;
    return false;
  }

  int retry_count = 0;
  struct cdb_phase cdb_phase;

  while (retry_count++ < 5) {
    auto ret = cdb_get_phase(sock, &cdb_phase);
    if (ret != CONFD_OK) {
      std::cerr << "Confd get phase failed!" << std::endl;
      return false;
    }
    if (cdb_phase.phase != 2) {
      sleep(1);
    }
  }

  cdb_get_phase(sock, &cdb_phase);
  std::cout << "Confd Phase: " << cdb_phase.phase << std::endl;
  if (cdb_phase.phase < 1) { // Confd phase-1 would also be enough for internal things
    return false;
  }
  return true;
}

void ConfdUnittestHarness::make_confd_sockaddr(
  sockaddr_t* sun)
{
  RW_ASSERT(confd_pid_); // zero indicates a race with start(), and therefore threads, and therefore a usage error
  memset (sun, 0, sizeof (sockaddr_t));
  sun->sun_family = AF_UNIX;
  const std::string& path = socket_path_.native();
  RW_ASSERT(path.length() && path.length() < sizeof(sun->sun_path));
  strncpy(sun->sun_path, path.c_str(), path.length());
}

confd_daemon_ctx* ConfdUnittestHarness::get_confd_context()
{
  if (confd_context_) {
    return confd_context_;
  }

  confd_init(test_name_.c_str(), stderr, CONFD_DEBUG);

  confd_context_ = confd_init_daemon(test_name_.c_str());
  RW_ASSERT(confd_context_);

  return confd_context_;
}

int ConfdUnittestHarness::alloc_confd_socket()
{
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  RW_ASSERT(fd >= 0);
  return fd;
}


void ConfdUnittestHarness::handle_sig(int sig, siginfo_t* si, void* uc)
{}


