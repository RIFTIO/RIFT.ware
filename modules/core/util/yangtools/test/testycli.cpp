/* 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 * */

/**
 * @file testycli.hpp
 * @author Tom Seidenberg (tom.seidenberg@riftio.com)
 * @date 2014/03/16
 * @brief Tests for yangcli.[ch]pp
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>

#include "yangcli.hpp"
#include "yangncx.hpp"

#include "rwut.h"

namespace rw_yang {


/**
 * rw_yang::BaseCli testing class.
 */
class TestBaseCli
: public BaseCli
{
  public:
    TestBaseCli(YangModel& model, bool debug);
    ~TestBaseCli();

  public:
    // Overrides of the base class.
    bool handle_command(ParseLineResult* r);
  public:
    rw_status_t interactive();
    rw_status_t interactive_nb();
    void interactive_quit();
    
  public:
    /// Interactive editline object, created only when needed
    CliEditline* editline_;

    /// Enable debugging by printing extra stuff
    bool debug_;


};

/**
 * Helper function to make a fully-type cli.
 */
TestBaseCli& tcli(BaseCli& cli)
{
  return static_cast<TestBaseCli&>(cli);
}


/**
 * Create a TestBaseCli object.
 */
TestBaseCli::TestBaseCli(YangModel& model, bool debug)
  : BaseCli(model),
    editline_(NULL),
    debug_(debug)
{
  ModeState::ptr_t new_root_mode(new ModeState(*this,*root_parse_node_));
  mode_stack_root(std::move(new_root_mode));

  add_builtins();
  setprompt();
}

/**
 * Destroy TestBaseCli object and all related objects.
 */
TestBaseCli::~TestBaseCli()
{
  if (editline_) {
    delete editline_;
  }
}



/**
 * Handle a command callback.  This callback will receive notification
 * of all syntactically correct command lines.  It should return false
 * if there are any errors.
 */
bool TestBaseCli::handle_command(ParseLineResult* r)
{
  // Look for builtin commands
  if (r->line_words_.size() > 0) {

    if (r->line_words_[0] == "exit" ) {
      RW_BCLI_ARGC_CHECK(r,1);
      mode_exit();
      return true;
    }

    if (r->line_words_[0] == "end" ) {
      RW_BCLI_ARGC_CHECK(r,1);
      mode_end_even_if_root();
      return true;
    }

    if (r->line_words_[0] == "quit" ) {
      RW_BCLI_ARGC_CHECK(r,1);
      interactive_quit();
      return true;
    }

    if (r->line_words_[0] == "start-cli" ) {
      RW_BCLI_ARGC_CHECK(r,1);
      rw_status_t rs = interactive();
      RW_BCLI_RW_STATUS_CHECK(rs);
      return true;
    }
  }

  std::cout << "Parsing tree:" << std::endl;
  r->parse_tree_->print_tree(4,std::cout);

  std::cout << "Result tree:" << std::endl;
  r->result_tree_->print_tree(4,std::cout);

  std::cout << "Unrecognized command" << std::endl;
  return false;
}


/**
 * Run an interactive CLI.  Does not allow recursion - only one
 * interactive CLI can be active at the same time.  This function will
 * not return until the user exits the CLI.
 *
 * A CLI can only be started from the root mode.  If a parser wants to
 * start a new CLI (for example, when parsing the command line
 * arguments, one of the arguments requests an interactive CLI), a
 * whole new TestBaseCli object should be created and the CLI started on
 * the new object, from the root context.
 */
rw_status_t TestBaseCli::interactive()
{
  if (editline_) {
    std::cerr << "CLI is already running" << std::endl;
    return RW_STATUS_FAILURE;
  }

  if (current_mode_ != root_mode_) {
    std::cerr << "Cannot start CLI outside of root mode context" << std::endl;
    return RW_STATUS_FAILURE;
  }

  editline_ = new CliEditline( *this, stdin, stdout, stderr );
  editline_->readline_loop();
  delete editline_;
  editline_ = NULL;
  mode_end_even_if_root();
  return RW_STATUS_SUCCESS;
}

/**
 * Run an interactive CLI in non-blocking mode.  Does not allow
 * recursion - only one interactive CLI can be active at the same time.
 * This function will not return until the user exits the CLI.
 *
 * A CLI can only be started from the root mode.  If a parser wants to
 * start a new CLI (for example, when parsing the command line
 * arguments, one of the arguments requests an interactive CLI), a
 * whole new TestBaseCli object should be created and the CLI started on
 * the new object, from the root context.
 */
rw_status_t TestBaseCli::interactive_nb()
{
  if (editline_) {
    std::cerr << "CLI is already running" << std::endl;
    return RW_STATUS_FAILURE;
  }

  if (current_mode_ != root_mode_) {
    std::cerr << "Cannot start CLI outside of root mode context" << std::endl;
    return RW_STATUS_FAILURE;
  }

  editline_ = new CliEditline( *this, stdin, stdout, stderr );
  editline_->nonblocking_mode();

  int fd = fileno(stdin);
  fd_set fds = {0};
  editline_->read_ready();  // Yes, gratuitous read_ready is needed

  while (!editline_->should_quit()) {
    editline_->read_ready();
    FD_SET(fd, &fds);
    int nfds = select( fd+1, &fds, NULL, NULL, NULL );
    if (nfds == 1) {
      RW_ASSERT(FD_ISSET(fd, &fds));
    } else if(nfds == -1) {
      switch (errno) {
        case EINTR:
          continue;
        case EBADF:
        case EINVAL:
        default:
          std::cerr << "Unexpected errno " << errno << std::endl;
          editline_->quit();
      }
    } else {
      RW_ASSERT_NOT_REACHED(); // nfds == 0?
    }
  }

  delete editline_;
  editline_ = NULL;
  mode_end_even_if_root();
  return RW_STATUS_SUCCESS;
}

/**
 * Quit the interactive CLI.
 */
void TestBaseCli::interactive_quit()
{
  if (editline_) {
    editline_->quit();
  }
}

} /* namespace rw_yang */

using namespace rw_yang;

char **g_argv;
int g_argc;

void mySetup(int argc, char **argv)
{
  bool want_debug = true;

  if( argc >= 2 ) {
    if (0 == strcmp(argv[1], "--cli")) {
      YangModelNcx* model = YangModelNcx::create_model();
      model->load_module("testycli-ia");
      TestBaseCli tbcli(*model,want_debug);

      // ATTN: Reaching in here is wrong - should have accessors.
      tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::AUTO_MODE_CFG_CONT);
      tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::AUTO_MODE_CFG_LISTK);

      tbcli.interactive();
      delete model;
      exit(0);

    } else if (0 == strcmp(argv[1], "--nbcli")) {
      YangModelNcx* model = YangModelNcx::create_model();
      model->load_module("testycli-ia");
      TestBaseCli tbcli(*model,want_debug);

      // ATTN: Reaching in here is wrong - should have accessors.
      tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::AUTO_MODE_CFG_CONT);
      tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::AUTO_MODE_CFG_LISTK);

      tbcli.interactive_nb();
      delete model;
      exit(0);
    }

    g_argv = argv;
    g_argc = argc;

  }
}

RWUT_INIT(mySetup);

