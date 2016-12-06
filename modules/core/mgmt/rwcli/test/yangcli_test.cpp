/* 
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
 * */

/**
 * @file testycli.hpp
 * @author Tom Seidenberg (tom.seidenberg@riftio.com)
 * @date 2014/03/16
 * @brief Tests for yangcli.[ch]pp
 */

// This file was previously in the following path
//   modules/core/utils/yangtools/test/yangcli_test.cpp
// For commit history refer the submodule modules/core/utils

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include "gmock/gmock.h"
#include "gtest/rw_gtest.h"

#include "rw_app_data.hpp"
#include "yangcli.hpp"
#include "yangncx.hpp"

namespace rw_yang {

class TestBaseCli;

typedef bool (TestBaseCli::*TestBaseCliCB) (const ParseLineResult& r);

class CallbackTest:public CliCallback
{
 public:
  TestBaseCliCB       cb_func_;  /**< The function to be called when execute is called */
  TestBaseCli&        cb_data_;  /**< Object on which the callback is to be executed */


 public:
  CallbackTest(TestBaseCli& data, TestBaseCliCB func)
      :cb_func_(func), cb_data_(data) {}
  virtual bool execute(const ParseLineResult& r) {return (cb_data_.*cb_func_)(r);};
};

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
  void handle_parse_error (const parse_result_t result,
                           const std::string line);

  void mode_enter(ParseLineResult* r);
  void mode_enter(ParseNode::ptr_t&& result_tree, ParseNode* result_node);

  bool config_exit();

 public:
  // testing convienence functions
  bool enter_config_mode();
  bool exit_config_mode(bool last_mode_was_config = true);
  void end_config_mode();
  bool is_show_available();
  bool is_no_available();
  void set_rwcli_like_params();
  bool set_mode(const char* path, const char *display);
  bool set_and_enter_mode (const char *path, const char *display);
  bool first_level_cb (const ParseLineResult& r);
  bool second_level_cb (const ParseLineResult& r);
  bool load_config (const ParseLineResult& r) ;
  XMLDocument::uptr_t get_command_dom (ParseNode* pn);

  XMLDocument::uptr_t get_command_dom_from_result(ParseNode* result_tree);

 public:
  /// Test document creation
  XMLManager::uptr_t xmlmgr_;
  XMLDocument::uptr_t config_;

  /// Enable debugging by printing extra stuff
  bool debug_;

  /// mode strings
  std::list<std::string> modes_strings_;

  uint8_t lvl_1_;
  uint8_t lvl_2_;

  std::string error_line_;
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
  xmlmgr_(xml_manager_create_xerces(&model)),
  debug_(debug),
  lvl_1_(0),
  lvl_2_(0)
{
  config_ = std::move(xmlmgr_->create_document());

  ModeState::ptr_t new_root_mode(new ModeState(*this,*root_parse_node_));
  new_root_mode->set_xml_node(config_->get_root_node());
  mode_stack_root(std::move(new_root_mode));

  add_builtins();
  add_behaviorals();
  setprompt();
}


/**
 * Destroy TestBaseCli object and all related objects.
 */
TestBaseCli::~TestBaseCli()
{
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
  }

  // ATTN: Print the parse tree, to help debugging...
  std::cout << "Parsing tree:" << std::endl;
  r->parse_tree_->print_tree(4,std::cout);

  // ATTN: Print the result tree, to help debugging...
  std::cout << "Result tree:" << std::endl;
  r->result_tree_->print_tree(4,std::cout);

  std::cout << "Unhandled command" << std::endl;
  return true;
}


void TestBaseCli::mode_enter(ParseLineResult* r)
{
  if (r->result_node_->ignore_mode_entry()) {
    // The config behavior node sets mode_entry_ignore.

    // There should be more checks or a better way of doing this - but for now, this
    // is the best
    state_ = BaseCli::STATE_CONFIG;


    // Reset flags if only mode paths were allowed
    current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::MODE_PATH);
    current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::CONFIG_ONLY);
    
    // All show commands in config mode must print in config cli format
    // Emulates the RwCli behavior
    show_node_->set_cli_print_hook("test:config_print_hook");
    show_node_->flags_.set(ParseFlags::PRINT_HOOK_STICKY);

    return;
  }

  XMLNode *mode_xml_head = current_mode_->update_xml_cfg(config_.get(), r->result_tree_);
  RW_ASSERT (mode_xml_head);
  ModeState::ptr_t new_mode(new ModeState (r, mode_xml_head));

  mode_enter_impl(std::move(new_mode));

}

/**
 * Handle the base-CLI mode_enter callback.  Dispatch to the owning
 * RwCLI object.
 */
void TestBaseCli::mode_enter(ParseNode::ptr_t&& result_tree,
                            ParseNode* result_node)
{
  XMLNode *mode_xml_head = current_mode_->update_xml_cfg(config_.get(), result_tree);
  RW_ASSERT (mode_xml_head);
  ModeState::ptr_t new_mode(new ModeState (std::move(result_tree), result_node, mode_xml_head));
  mode_enter_impl(std::move(new_mode));

}

bool TestBaseCli::config_exit()
{
  // Reset the show node print hook attributes
  // Emulate RwCli behavior
  show_node_->set_cli_print_hook(nullptr);
  show_node_->flags_.clear(ParseFlags::PRINT_HOOK_STICKY);  
  return true;
}


void TestBaseCli::handle_parse_error (const parse_result_t result,
                                      const std::string line)
{
  error_line_ = line;
}

bool TestBaseCli::enter_config_mode()
{

  size_t depth = mode_stack_.size();
  ParseNode *pn = current_mode_->top_parse_node_;
  ParseNode *completion;
  ParseLineResult r ( *this, "config" ,ParseLineResult::ENTERKEY_MODE);

  if (!r.success_) { return false;}
  if (r.completions_.size() != 1) { return false;}

  completion = r.completions_[0].node_;

  if (completion->is_key()) return false;

  if (completion->has_keys()) return false;

  // ret, completion and result should be the same.
  if (BaseCli::NONE != state_) return false;
  // the new top should be the same as the old top, just a clone

  mode_enter (&r);

  if (BaseCli::STATE_CONFIG != state_) return false;
  if (depth !=  mode_stack_.size()) return false;
  if (pn != current_mode_->top_parse_node_) return false;

  return true;
}

bool TestBaseCli::exit_config_mode(bool last_mode_was_config)
{
  // The mode stack and root parse node should be the same after exiting
  size_t depth = mode_stack_.size();
  ParseNode *pn = current_mode_->top_parse_node_;
  ParseLineResult r (*this, std::string("exit"),ParseLineResult::ENTERKEY_MODE);

  if (!r.success_) { return false;}
  if (!r.completions_.size() == 1) { return false;}

  mode_exit ();

  if (last_mode_was_config) {
    if (depth != mode_stack_.size()) { return false; }
    if (BaseCli::NONE !=  state_) { return false;}
    if (pn != current_mode_->top_parse_node_) { return false;}
  }

  return true;
}


void TestBaseCli::end_config_mode()
{
  // The mode stack and root parse node should be the same after exiting
  ParseLineResult r (*this, std::string("end"),ParseLineResult::ENTERKEY_MODE);
  ParseNode *completion;

  EXPECT_EQ (r.success_, true);
  EXPECT_EQ (r.completions_.size(), 1);

  completion = r.completions_[0].node_;

  EXPECT_FALSE (completion->is_key());
  EXPECT_FALSE (completion->is_leafy());
  EXPECT_FALSE (completion->is_config());
  EXPECT_TRUE (completion->is_keyword());

  EXPECT_TRUE (completion->is_sentence());
  EXPECT_FALSE (completion->has_keys());
  EXPECT_FALSE (completion->is_mode());

  mode_end_even_if_root();

  ASSERT_EQ (1, mode_stack_.size());
  ASSERT_EQ (BaseCli::NONE, state_);
}

bool TestBaseCli::is_show_available()
{
  // show should always be available
  ParseLineResult t(*this, "show", ParseLineResult::ENTERKEY_MODE);
  ParseNode *completion;
  EXPECT_EQ (t.success_, true);
  EXPECT_EQ (t.completions_.size(), 1);

  completion = t.completions_[0].node_;

  EXPECT_TRUE (!completion->is_key());
  EXPECT_TRUE (!completion->is_leafy());
  EXPECT_TRUE (!completion->is_config());
  EXPECT_TRUE (completion->is_keyword());

  EXPECT_TRUE (!completion->is_sentence());
  EXPECT_TRUE (!completion->has_keys());
  EXPECT_TRUE (!completion->is_mode());
  return true;

}
bool TestBaseCli::is_no_available()
{
  // show should always be available
  ParseLineResult t(*this, std::string("no"),ParseLineResult::ENTERKEY_MODE);
  ParseNode *completion;
  if (!t.success_) {
    return false;
  }

  EXPECT_EQ (t.success_, true);
  EXPECT_EQ (t.completions_.size(), 1);

  completion = t.completions_[0].node_;

  EXPECT_TRUE (!completion->is_key());
  EXPECT_TRUE (!completion->is_leafy());
  EXPECT_TRUE (!completion->is_config());
  EXPECT_TRUE (completion->is_keyword());

  EXPECT_TRUE (!completion->is_sentence());
  EXPECT_TRUE (!completion->has_keys());
  EXPECT_TRUE (!completion->is_mode());
  return true;

}

bool TestBaseCli::set_mode(const char* path, const char *display)
{
  // save off current flags
  ParseFlags save = current_mode_->top_parse_node_->flags_;
  ParseFlags tmp;
  tmp.set(ParseFlags::V_LIST_KEYS_CLONED);
  current_mode_->top_parse_node_->flags_ = tmp;

  ParseLineResult r(*this, path, ParseLineResult::ENTERKEY_MODE);
  current_mode_->top_parse_node_->flags_ = save;

  if (!r.success_) {
    return false;
  }

  if (r.result_node_->is_leafy()) {
    return false;
  }

  if (nullptr == display) {
    display = r.result_node_->value_.c_str();
  }
  std::string display_str= std::string(display);

  modes_strings_.push_back(display_str);

  r.result_node_->set_mode (display_str.c_str());

  return true;
}

bool TestBaseCli::set_and_enter_mode (const char *path, const char *display)
{
  if (!set_mode (path, display)) {
    return false;
  }

  ParseLineResult r (*this, std::string(path), ParseLineResult::ENTERKEY_MODE);
  mode_enter (&r);

  return true;
}
void TestBaseCli::set_rwcli_like_params()
{
  root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);
  root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::MODE_PATH);

  root_mode_->top_parse_node_->flags_.set(ParseFlags::PARSE_TOP);
  root_mode_->top_parse_node_->flags_.set(ParseFlags::CLI_ROOT);
}

bool TestBaseCli::first_level_cb(const ParseLineResult& r)
{
  UNUSED (r);
  lvl_1_++;
  return true;
}

bool TestBaseCli::second_level_cb (const ParseLineResult& r)
{
  UNUSED (r);
  lvl_2_ ++;
  return true;
}

bool TestBaseCli::load_config (const ParseLineResult& r) {
  // decide what needs to be done here
  UNUSED (r);
  lvl_2_ ++;
  return true;
}

XMLDocument::uptr_t TestBaseCli::get_command_dom(ParseNode* result_node)
{
  XMLDocument::uptr_t command(xmlmgr_->create_document());
  XMLNode *parent_xml = command->get_root_node();
  ModeState::stack_iter_t iter = mode_stack_.begin();
  iter++;

  while (mode_stack_.end() != iter) {
    parent_xml = iter->get()->create_xml_node (command.get(), parent_xml);
    iter++;
  }

  iter->get()->merge_xml (command.get(), parent_xml, result_node);
  return (command);
}

XMLDocument::uptr_t TestBaseCli::get_command_dom_from_result(ParseNode* result_tree)
{
  XMLDocument::uptr_t command = std::move(xmlmgr_->create_document());
  XMLNode *parent_xml = command->get_root_node();
  ModeState::stack_iter_t iter = mode_stack_.begin();
  iter++;

  while (mode_stack_.end() != iter) {
    parent_xml = iter->get()->create_xml_node (command.get(), parent_xml);
    iter++;
  }

  for (ParseNode::child_iter_t result_node = result_tree->children_.begin();
       result_node != result_tree->children_.end(); result_node ++)
  {
    iter->get()->merge_xml (command.get(), parent_xml, result_node->get());
  }
  return (command);
}


} /* namespace rw_yang */



const char  *g_container="general-container g-container ";
const char  *g_list="general-container g-list ";

const char  *ll_int_ends[] = {"gc-l", "gc-leaf-l", "gc-leaf-list-unit8"};
const char *gc_n_starts[]  = {"gc-n", "gc-na", "gc-name"};
const char  *uint8_values_valid[] = {"0", "100", "255"};
const char  *uint8_values_invalid[] = {"-1", "foo", "256"};

const char  *gc_sub_starts[] = {"gc-s", "gc-sub", "gc-sub-container"};
const char *gc_sub_valid_ends[] = {"gc-sub-container gc-sub-cont-in 15", "gc-sub-container gc-sub-cont-int 15"};
const char *gc_sub_invalid_ends[] = {"gc-sub-container gc-sub-cont-int foo"};

const char *some_strings[] = {"foo", "akl9324*@#$_"};

const char* gn_container_paths[] = {"", "gc-leaf-list-unit8",
                                  "gc-sub-container" ,
                                  "gc-sub-container gc-sub-cont-int" ,
                                  "int32-leaf" ,
                                  "int32-list" ,
                                  "double-key" ,
                                  "double-key 3"
};
const char* gn_container_xml_paths[] = {"", "gc-leaf-list-unit8",
                                  "gc-sub-container" ,
                                  "gc-sub-container gc-sub-cont-int" ,
                                  "int32-leaf" ,
                                  "int32-list" ,
                                  "double-key" ,
                                  "double-key index 3"
};

const char* gn_container_trunc[] = {"gc-leaf-list-unit",
                                  "gc-sub-containe" ,
                                  "gc-sub-container gc-sub-cont-in" ,
                                  "int32-lea" ,
                                  "int32-lis" ,
                                  "double-ke" ,
};

const char* gn_container_paths_val[] = {"gc-leaf-list-unit8 3",
                                      "gc-sub-container gc-sub-cont-int 45" ,
                                      "int32-leaf 2455" ,
                                      "int32-list 3465" ,
                                      "double-key 3 name first"
};

const char* gn_container_paths_val_no[] = {
  "gc-sub-container gc-sub-cont-int 45" ,
  "int32-leaf 2455" ,
  "double-key 3 name first"
};

const char *gs_paths[] = {"general-show",
                          "general-show g-container",
                          "general-show g-container gc-name",
                          "general-show g-container gc-name foobar"
};

using namespace rw_yang;

extern YangModelNcx* model;

bool validate_leaf (ParseNode *pn) {
  if (!pn->is_leafy()) {return false;}
  if  (!pn->is_keyword()) {return false;}
  if (pn->has_keys()) {return  false;};

  return true;
}

TEST (YangCLIBehavioral, Config_Enter_Exit)
{
  TEST_DESCRIPTION("Test basic config behavioral node");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ParseLineResult a(tbcli, std::string("g"),ParseLineResult::NO_OPTIONS);
  EXPECT_TRUE (a.success_);  // Success doesnt mean much with NO_OPTIONS
  EXPECT_EQ (0, a.completions_.size());

  ASSERT_TRUE(tbcli.enter_config_mode());

  // config should not be an allowed value in the config mode
  ParseLineResult b(tbcli, "config", ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE (!b.success_);

  // no and show should be availabe,

  ASSERT_TRUE (tbcli.is_show_available());
  ASSERT_TRUE (tbcli.is_no_available());

  // and the general container
  ParseLineResult c(tbcli, std::string("general-container"),ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE (c.success_);
  ASSERT_EQ (1, c.completions_.size());

  XMLDocument::uptr_t dom(tbcli.get_command_dom(c.result_tree_->children_.begin()->get()));
  XMLNode* root = dom->get_root_node();

  // Advance root to the next node
  XMLNode* fchild = root->get_first_child();

  ASSERT_NE (root, nullptr);
  ASSERT_EQ (1, root->get_children()->length());
  ASSERT_NE (fchild, nullptr);
  ASSERT_STREQ(fchild->get_local_name().c_str(), "general-container");

  // There are 2 things that start with "g", but only one is config.
  ParseLineResult d (tbcli, std::string("g"),ParseLineResult::NO_OPTIONS);
  EXPECT_TRUE (d.success_);
  EXPECT_EQ (1, d.completions_.size());

  // test exiting
  ASSERT_TRUE(tbcli.exit_config_mode());

  // config should be available again.
  ParseLineResult e(tbcli, "config", ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE (e.success_);

  // no should not be avaialble outside of config mode
  ASSERT_TRUE (!tbcli.is_no_available());

  //Show should be though
  ASSERT_TRUE (tbcli.is_show_available());

  //There are no modes, so nothing starting with "g" should succeed for now.
  ParseLineResult f(tbcli, std::string("g"),ParseLineResult::NO_OPTIONS);
  EXPECT_TRUE (f.success_);  // Success doesnt mean much with NO_OPTIONS
  EXPECT_EQ (0, f.completions_.size());
}

TEST (YangCLIBehavioral, Config_Enter_End)
{
  TEST_DESCRIPTION("Test Config enter and end");

  // After config and end, the config nodes should not be added oto the root
  // node level - RIFT-2439

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ParseLineResult r(tbcli, "", ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE (r.success_);

  int op_completions = r.completions_.size();

  ASSERT_TRUE(tbcli.enter_config_mode());

  ParseLineResult cr(tbcli, "", ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE (cr.success_);

  tbcli.end_config_mode();

  // Check for the MODE_PATH and !CONFIG_TRUE flags on the root_node
  bool is_mode_path = tbcli.current_mode_->top_parse_node_->flags_.inherit_.test(
                            ParseFlags::to_index(ParseFlags::MODE_PATH));
  bool is_config = tbcli.current_mode_->top_parse_node_->flags_.inherit_.test(
                            ParseFlags::to_index(ParseFlags::CONFIG_ONLY));
  EXPECT_TRUE(is_mode_path);
  EXPECT_FALSE(is_config);

  ParseLineResult er(tbcli, "", ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE (er.success_);

  int op_completions_again = er.completions_.size();
  EXPECT_EQ(op_completions, op_completions_again);

}

TEST (YangCLIBehavioral, Config_Completions_LeafsLists)
{
  TEST_DESCRIPTION("Test completions scenarios for config behavioral node");
  ParseNode *completion;
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());

  for (size_t i = 0; i < sizeof (ll_int_ends)/sizeof (char *); i++) {

    ParseLineResult r (tbcli, std::string(g_container) + std::string(ll_int_ends[i]),
                       ParseLineResult::NO_OPTIONS );

    ASSERT_TRUE (r.success_);
    EXPECT_EQ (r.completions_.size(), 1);
    EXPECT_EQ (r.completions_[0].node_->get_stmt_type(), RW_YANG_STMT_TYPE_LEAF_LIST);
    EXPECT_TRUE(validate_leaf (r.completions_[0].node_));
    EXPECT_EQ (r.completions_[0].node_->is_key(), false);
    EXPECT_TRUE (r.completions_[0].node_->is_config());
    EXPECT_EQ (r.completions_[0].node_->is_sentence(), false);
  }

  for (size_t i = 0; i < sizeof (uint8_values_valid)/sizeof (char *); i++) {
    ParseLineResult a (tbcli, std::string(g_container) + std::string("gc-leaf-list-unit8 ") +
                       std::string(uint8_values_valid[i]), ParseLineResult::ENTERKEY_MODE );
    ASSERT_TRUE (a.success_);
    ASSERT_EQ (a.completions_.size(), 1);

    completion = a.completions_[0].node_;
    ASSERT_TRUE (completion->is_sentence());

    XMLDocument::uptr_t dom(tbcli.get_command_dom(a.result_tree_->children_.begin()->get()));
    XMLNode* root = dom->get_root_node();

    // Advance root to the next node
    XMLNode* fchild = root->get_first_child();

    ASSERT_NE (root, nullptr);
    ASSERT_EQ (1, root->get_children()->length());
    ASSERT_NE (fchild, nullptr);

    ASSERT_STREQ(fchild->get_local_name().c_str(), "general-container");
    ASSERT_EQ (1, fchild->get_children()->length());
    fchild=fchild->get_children()->at(0);

    ASSERT_STREQ(fchild->get_local_name().c_str(), "g-container");
    ASSERT_EQ (1, fchild->get_children()->length());
    fchild=fchild->get_children()->at(0);

    ASSERT_STREQ(fchild->get_local_name().c_str(), "gc-leaf-list-unit8");
    ASSERT_EQ (1, fchild->get_children()->length());

    EXPECT_STREQ(fchild->get_text_value().c_str(), uint8_values_valid[i]);
  }

  for (size_t i = 0; i < sizeof (uint8_values_invalid)/sizeof (char *); i++) {
    ParseLineResult b (tbcli,  std::string(g_container) + std::string("gc-leaf-list-unit8 ") +
                       std::string(uint8_values_invalid[i]), ParseLineResult::ENTERKEY_MODE );
    ASSERT_TRUE (!b.success_);
  }

  // Test it after adding a mode at g-container level in configuration mode
  tbcli.set_and_enter_mode(g_container, nullptr);

  for (size_t i = 0; i < sizeof (uint8_values_valid)/sizeof (char *); i++) {
    ParseLineResult c (tbcli, std::string("gc-leaf-list-unit8 ") +
                       std::string(uint8_values_valid[i]), ParseLineResult::ENTERKEY_MODE );
    ASSERT_TRUE (c.success_);
    ASSERT_EQ (c.completions_.size(), 1);

    completion = c.completions_[0].node_;
    ASSERT_TRUE (completion->is_sentence());

    XMLDocument::uptr_t dom(tbcli.get_command_dom(c.result_tree_->children_.begin()->get()));
    XMLNode* root = dom->get_root_node();

    // Advance root to the next node
    XMLNode* fchild = root->get_first_child();

    ASSERT_NE (root, nullptr);
    ASSERT_EQ (1, root->get_children()->length());
    ASSERT_NE (fchild, nullptr);

    ASSERT_STREQ(fchild->get_local_name().c_str(), "general-container");
    ASSERT_EQ (1, fchild->get_children()->length());
    fchild = fchild->get_children()->at(0);

    ASSERT_STREQ(fchild->get_local_name().c_str(), "g-container");
    ASSERT_EQ (1, fchild->get_children()->length());
    fchild = fchild->get_children()->at(0);

    ASSERT_STREQ(fchild->get_local_name().c_str(), "gc-leaf-list-unit8");
    ASSERT_EQ (1, fchild->get_children()->length());

    ASSERT_STREQ(fchild->get_text_value().c_str(), uint8_values_valid[i]);
  }

  for (size_t i = 0; i < sizeof (ll_int_ends)/sizeof (char *); i++) {

    ParseLineResult d (tbcli,std::string(ll_int_ends[i]), ParseLineResult::NO_OPTIONS );

    ASSERT_TRUE (d.success_);
    EXPECT_EQ (d.completions_.size(), 1);
    EXPECT_EQ (d.completions_[0].node_->get_stmt_type(), RW_YANG_STMT_TYPE_LEAF_LIST);
    EXPECT_TRUE(validate_leaf (d.completions_[0].node_));
  }
  ASSERT_TRUE(tbcli.exit_config_mode(false));
}

TEST (YangCLIBehavioral, Config_Completions_Leafs_Strings)
{
  TEST_DESCRIPTION("Test completions scenarios for config behavioral node");
  ParseNode *completion;
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());

  for (size_t i = 0; i < sizeof (gc_n_starts)/sizeof (char *); i++) {

    ParseLineResult r (tbcli, std::string(g_container) + std::string(gc_n_starts[i]),
                       ParseLineResult::NO_OPTIONS );

    ASSERT_TRUE (r.success_);
    EXPECT_EQ (r.completions_.size(), 1);
    EXPECT_EQ (r.completions_[0].node_->get_stmt_type(), RW_YANG_STMT_TYPE_LEAF);
    EXPECT_TRUE(validate_leaf (r.completions_[0].node_));
    EXPECT_EQ (r.completions_[0].node_->is_key(), false);
    EXPECT_TRUE (r.completions_[0].node_->is_config());
    EXPECT_EQ (r.completions_[0].node_->is_sentence(), false);
  }

  for (size_t i = 0; i < sizeof (some_strings)/sizeof (char *); i++) {
    ParseLineResult a (tbcli, std::string(g_container) + std::string("gc-name ") +
                       std::string(some_strings[i]), ParseLineResult::ENTERKEY_MODE );
    ASSERT_TRUE (a.success_);
    ASSERT_EQ (a.completions_.size(), 1);

    completion = a.completions_[0].node_;
    ASSERT_TRUE (completion->is_sentence());
  }
  ASSERT_TRUE(tbcli.exit_config_mode());
}

TEST (YangCLIBehavioral, Config_Completions_Containers)
{
  TEST_DESCRIPTION("Test completions scenarios for config behavioral node");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());

  ParseNode *completion;
  for (size_t i = 0; i < sizeof (gc_sub_starts)/sizeof (char *); i++) {

    ParseLineResult r (tbcli, std::string(g_container) + std::string(gc_sub_starts[i]),
                       ParseLineResult::NO_OPTIONS );
    EXPECT_EQ (r.success_, true);
    EXPECT_EQ (r.completions_.size(), 1);
    EXPECT_EQ (r.completions_[0].node_->get_stmt_type(), RW_YANG_STMT_TYPE_CONTAINER);
    EXPECT_EQ (r.completions_[0].node_->is_key(), false);
    EXPECT_TRUE (!r.completions_[0].node_->is_leafy());
    EXPECT_TRUE (r.completions_[0].node_->is_config());
    EXPECT_TRUE (r.completions_[0].node_->is_keyword());

    EXPECT_EQ (r.completions_[0].node_->is_sentence(), false);
    EXPECT_EQ (r.completions_[0].node_->has_keys(), false);

  }

  for (size_t i = 0; i < sizeof (gc_sub_valid_ends)/sizeof (char *); i++) {
    ParseLineResult a (tbcli, std::string(g_container) +
                       std::string(gc_sub_valid_ends[i]), ParseLineResult::ENTERKEY_MODE );
    ASSERT_TRUE (a.success_);
    ASSERT_EQ (a.completions_.size(), 1);

    completion = a.completions_[0].node_;
    ASSERT_TRUE (completion->is_sentence());
  }

  for (size_t i = 0; i < sizeof (gc_sub_invalid_ends)/sizeof (char *); i++) {
    ParseLineResult b (tbcli, std::string(g_container) +
                       std::string(gc_sub_invalid_ends[i]), ParseLineResult::ENTERKEY_MODE );
    ASSERT_TRUE (!b.success_);
  }
}

TEST (YangCLIBehavioral, Config_Negatives)
{
  TEST_DESCRIPTION("Test negative scenarios for config behavioral node");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);

  tbcli.set_rwcli_like_params();

  // nothing should be present after the config keyword
  ParseLineResult a(tbcli, "config g", ParseLineResult::NO_OPTIONS);
  EXPECT_TRUE (a.success_);  // Success doesnt mean much with NO_OPTIONS
  EXPECT_EQ (0, a.completions_.size());
}

TEST (YangCLIBehavioral, ParseError)
{
  TEST_DESCRIPTION("Test negative scenarios for config behavioral node");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);

  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());
    // and the general container
  tbcli.parse_line_buffer(std::string("gnral-container"), false);
  EXPECT_EQ (tbcli.error_line_, "gnral-container");
}


TEST (YangCLIBehavioral, Mode_Paths_Basic)
{
  TEST_DESCRIPTION("Test basic scenarios for modepath behavioral node");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);

  tbcli.set_rwcli_like_params();

  ParseLineResult a(tbcli, std::string("g"),ParseLineResult::NO_OPTIONS);
  EXPECT_TRUE (a.success_);  // Success doesnt mean much with NO_OPTIONS
  EXPECT_EQ (0, a.completions_.size());

  // set a mode at general-container
  ASSERT_TRUE(tbcli.set_mode ("general-container", "goo"));

  ParseLineResult b(tbcli, std::string("g"),ParseLineResult::NO_OPTIONS);
  EXPECT_TRUE (b.success_);  // Success doesnt mean much with NO_OPTIONS
  EXPECT_EQ (1, b.completions_.size());

  // set a mode at general-show
  ASSERT_TRUE (tbcli.set_mode ("general-show", "shoo"));

  ParseLineResult c(tbcli, std::string("g"),ParseLineResult::NO_OPTIONS);
  EXPECT_TRUE (c.success_);  // Success doesnt mean much with NO_OPTIONS
  EXPECT_EQ (2, c.completions_.size());
}



TEST (YangCLIBehavioral, Show_Basic)
{
  TEST_DESCRIPTION("Test completions scenarios for config behavioral node");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();


  // show should have two completions
  ParseLineResult a (tbcli, "show g", ParseLineResult::NO_OPTIONS);

  ASSERT_TRUE (a.success_);
  ASSERT_EQ (a.completions_.size(), 2);

  ASSERT_TRUE(tbcli.enter_config_mode());
  // In config mode, show should show only one completion

  // show should have two completions
  ParseLineResult b (tbcli, "show g", ParseLineResult::NO_OPTIONS);

  ASSERT_TRUE (b.success_);
  ASSERT_EQ (b.completions_.size(), 1);

  // config mode - show should be successful at any level

  ParseLineResult c (tbcli, "show general-container", ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE (c.success_);
  ASSERT_EQ (c.completions_.size(), 1);
  ASSERT_TRUE (c.parse_tree_->is_sentence());

  XMLDocument::uptr_t dom(tbcli.get_command_dom(c.result_tree_->children_.begin()->get()));
  XMLNode* root = dom->get_root_node();

  // Advance root to the next node
  XMLNode* fchild = root->get_first_child();

  ASSERT_NE (root, nullptr);
  ASSERT_EQ (1, root->get_children()->length());
  ASSERT_NE (fchild, nullptr);

  ASSERT_STREQ(fchild->get_local_name().c_str(), "general-container");

  // single completion substrings - should be fine
  for (size_t i = 0; i < sizeof (gn_container_trunc)/sizeof (char *); i++) {
    ParseLineResult e (tbcli, std::string("show ") + std::string(g_container) +
                       std::string(gn_container_trunc[i]), ParseLineResult::ENTERKEY_MODE );

    ASSERT_TRUE (e.success_);
    ASSERT_TRUE (e.parse_tree_->is_sentence());
  }

    // invalid substrings
  for (size_t i = 0; i < sizeof (gn_container_paths_val)/sizeof (char *); i++) {
    ParseLineResult f (tbcli, std::string("show ") + std::string(g_container) +
                       std::string(gn_container_paths_val[i]), ParseLineResult::ENTERKEY_MODE );

    ASSERT_TRUE (!f.success_);
  }

  tbcli.set_and_enter_mode(g_container, nullptr);

  for (size_t i = 1; i < sizeof (gn_container_paths)/sizeof (char *); i++) {

    std::string cmd = std::string(gn_container_paths[i]);

    ParseLineResult g (tbcli, std::string("show ")
                       + cmd, ParseLineResult::ENTERKEY_MODE );


    ASSERT_TRUE (g.success_);
    ASSERT_TRUE (g.parse_tree_->is_sentence());

    XMLDocument::uptr_t dom(tbcli.get_command_dom(g.result_tree_->children_.begin()->get()));
    XMLNode* root = dom->get_root_node();

    // Advance root to the next node
    XMLNode* fchild = root->get_first_child();

    ASSERT_NE (root, nullptr);
    ASSERT_EQ (1, root->get_children()->length());
    ASSERT_NE (fchild, nullptr);

    ASSERT_STREQ(fchild->get_local_name().c_str(), "general-container");

    ASSERT_STREQ(fchild->get_local_name().c_str(), "general-container");
    ASSERT_EQ (1, fchild->get_children()->length());
    fchild = fchild->get_children()->at(0);

    ASSERT_STREQ(fchild->get_local_name().c_str(), "g-container");

    if (strlen(gn_container_xml_paths[i])) {

      char *first = 0, *second = 0, *third = 0;
      char *path = strdup(gn_container_xml_paths[i]);

      first = path;

      if ((second = (strchr (path, ' ')))) {
        *second = '\0';
        second++;

        if ((third = (strchr (second, ' ')))) {
          *third = '\0';
          third ++;
        }
      }

      ASSERT_EQ (1, fchild->get_children()->length());
      fchild = fchild->get_children()->at(0);
      ASSERT_STREQ(fchild->get_local_name().c_str(), first);

      if (second) {
        ASSERT_EQ (1, fchild->get_children()->length());
        fchild = fchild->get_children()->at(0);
        ASSERT_STREQ(fchild->get_local_name().c_str(), second);

        if (third)
        {
          ASSERT_STREQ(fchild->get_text_value().c_str(), third);
        }
      }
    }
  }

  //exit mode
  ASSERT_TRUE(tbcli.exit_config_mode(false));
  // and config
  ASSERT_TRUE(tbcli.exit_config_mode());

  // non - config paths - values should be ok
  for (size_t i = 0; i < sizeof (gs_paths)/sizeof (char *); i++) {
    ParseLineResult e (tbcli, std::string("show ") + std::string(gs_paths[i])
                       , ParseLineResult::ENTERKEY_MODE );

    //ASSERT_TRUE (e.success_);
  }

}

TEST (YangCLIBehavioral, No_Basic)
{
  TEST_DESCRIPTION("Test completions scenarios for config behavioral node");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();


  ASSERT_TRUE(tbcli.enter_config_mode());
  // In config mode, show should show only one completion

  // show should have two completions
  ParseLineResult b (tbcli, "no g", ParseLineResult::NO_OPTIONS);

  ASSERT_TRUE (b.success_);
  ASSERT_EQ (b.completions_.size(), 1);

  // config mode - show should be successful at any level

  ParseLineResult c (tbcli, "no general-container", ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE (c.success_);
  ASSERT_EQ (c.completions_.size(), 1);
  ASSERT_TRUE (c.parse_tree_->is_sentence());

  for (size_t i = 0; i < sizeof (gn_container_paths)/sizeof (char *); i++) {
    ParseLineResult d (tbcli, std::string ("no ") + std::string(g_container) +
                       std::string(gn_container_paths[i]), ParseLineResult::ENTERKEY_MODE );

    ASSERT_TRUE (d.success_);
    ASSERT_TRUE (d.parse_tree_->is_sentence());
  }

  // single completion substrings - should be fine
  for (size_t i = 0; i < sizeof (gn_container_trunc)/sizeof (char *); i++) {
    ParseLineResult e (tbcli, std::string("no ") + std::string(g_container) +
                       std::string(gn_container_trunc[i]), ParseLineResult::ENTERKEY_MODE );

    ASSERT_TRUE (e.success_);
    ASSERT_TRUE (e.parse_tree_->is_sentence());
  }

    // invalid substrings
  for (size_t i = 0; i < sizeof (gn_container_paths_val_no)/sizeof (char *); i++) {
    ParseLineResult f (tbcli, std::string ("no ") + std::string(g_container) +
                       std::string(gn_container_paths_val_no[i]), ParseLineResult::ENTERKEY_MODE );

    ASSERT_TRUE (!f.success_);
  }

  tbcli.set_and_enter_mode(g_container, nullptr);

  for (size_t i = 1; i < sizeof (gn_container_paths)/sizeof (char *); i++) {
    ParseLineResult g (tbcli, std::string ("no ") +
                       std::string(gn_container_paths[i]), ParseLineResult::ENTERKEY_MODE );

    ASSERT_TRUE (g.success_);
    ASSERT_TRUE (g.parse_tree_->is_sentence());
  }

  //exit mode
  ASSERT_TRUE(tbcli.exit_config_mode(false));
  // and config
  ASSERT_TRUE(tbcli.exit_config_mode());

  // non - config paths - values should be ok
  for (size_t i = 0; i < sizeof (gs_paths)/sizeof (char *); i++) {
    ParseLineResult e (tbcli, std::string("no ") + std::string(gs_paths[i])
                       , ParseLineResult::ENTERKEY_MODE );

    ASSERT_TRUE (!e.success_);
  }

}

TEST (YangCLI, Prompts)
{
  TEST_DESCRIPTION("Test completions scenarios for config behavioral node");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  tbcli.set_and_enter_mode(g_container, nullptr);
  std::string expected ("rift(g-container)# ");
  ASSERT_EQ (expected, tbcli.prompt_);

  EXPECT_TRUE(tbcli.exit_config_mode(false));
  tbcli.set_mode(g_list, g_list);

  EXPECT_TRUE (!strcmp (tbcli.prompt_, "rift# "));
  ParseLineResult r (tbcli, "general-container",
                     ParseLineResult::ENTERKEY_MODE);
  tbcli.mode_enter (&r);
  ASSERT_TRUE (!strcmp (tbcli.prompt_, "rift(general-container)# "));
#if 0
  ParseLineResult s(tbcli, "g-list index 4",
                    ParseLineResult::ENTERKEY_MODE);
  tbcli.mode_enter (&s);
  ASSERT_TRUE (!strcmp (tbcli.prompt_, "rift(g-list-4)# "));
#endif
  EXPECT_TRUE(tbcli.exit_config_mode(false));
  tbcli.set_and_enter_mode(g_container, "boo");
  expected =std::string ("rift(boo)# ");
  ASSERT_EQ (expected, tbcli.prompt_);


}


TEST (YangCLIRpc, Basic)
{
  TEST_DESCRIPTION("Test completions scenarios for rpc node");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ParseLineResult b (tbcli, "both-args i", ParseLineResult::NO_OPTIONS);
  EXPECT_EQ (b.success_, true);

  ParseLineResult c (tbcli, "both-args int-arg 400", ParseLineResult::ENTERKEY_MODE);
  EXPECT_TRUE (c.success_);
  EXPECT_TRUE (c.parse_tree_->is_sentence());

  ParseLineResult d (tbcli, "both-args int-arg ", ParseLineResult::ENTERKEY_MODE);
  EXPECT_TRUE (d.success_);
  EXPECT_TRUE (!d.parse_tree_->is_sentence());
  // specifying an output child should fail
  ParseLineResult e (tbcli, "both-args op str-arg boo", ParseLineResult::ENTERKEY_MODE);
  EXPECT_EQ (e.success_, false);
}

TEST (YangCLICase, Basic)
{
  TEST_DESCRIPTION("Test completions scenarios for config case node");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());

  ParseLineResult b (tbcli, "test-choice ", ParseLineResult::NO_OPTIONS);
  ASSERT_EQ (b.success_, true);
  EXPECT_EQ (b.completions_.size(), 9);

  // Adding a non-case node should remove one completion, itself
  ParseLineResult c (tbcli, "test-choice choice-top-sib-str something ",
                       ParseLineResult::NO_OPTIONS);
  EXPECT_EQ (c.success_, true);
  EXPECT_EQ (c.completions_.size(), 8);

  // Adding a case child - other case child should be removed from completions
  ParseLineResult d (tbcli, "test-choice case-top-1-1 something ",
                       ParseLineResult::NO_OPTIONS);
  EXPECT_EQ (d.success_, true);
  EXPECT_EQ (d.completions_.size(), 3);

  ParseLineResult e (tbcli, "test-choice case-top-1-2 something ",
                       ParseLineResult::NO_OPTIONS);
  EXPECT_EQ (e.success_, true);
  EXPECT_EQ (e.completions_.size(), 3);

  ParseLineResult f (tbcli, "test-choice case-top-1-2 something case-top-1-1 something choice-top-sib-str something  choice-top-sib-num 5 ",
                       ParseLineResult::ENTERKEY_MODE);
  EXPECT_TRUE (f.success_);
// The parse node will be reset to the root, since all the elements in test-choice
// are exhausted
  EXPECT_STREQ("data", f.parse_node_->token_text_get());

// Nodes from conflicting cases - should fail
ParseLineResult g(tbcli, "test-choice case-top-1-2 something case-top-2-1 something",
                       ParseLineResult::ENTERKEY_MODE);
EXPECT_TRUE (!g.success_);

ParseLineResult h(tbcli, "test-choice case-mid-2-1 something case-top-2-1 something",
                    ParseLineResult::ENTERKEY_MODE);
EXPECT_TRUE (h.success_);

ParseLineResult i(tbcli, "test-choice case-mid-2-1 something case-top-1-1 something",
                    ParseLineResult::ENTERKEY_MODE);
EXPECT_TRUE (!i.success_);


}

TEST (YangCLIFucntional, Basic)
{

  TEST_DESCRIPTION("Test completions scenarios for config behavioral node");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ParseNodeFunctional* show_config_node = new ParseNodeFunctional(tbcli, "config", tbcli.show_node_.get());

  std::string show_config_help = std::string("show the configuration of the system");
  show_config_node->help_short_set (show_config_help.c_str());
  show_config_node->help_full_set (show_config_help.c_str());
  show_config_node->flags_.set_inherit (ParseFlags::HIDE_MODES);
  show_config_node->flags_.set_inherit (ParseFlags::HIDE_VALUES);

  show_config_node->set_callback( new CallbackTest (tbcli,&TestBaseCli::first_level_cb));

  tbcli.show_node_->add_descendent (show_config_node);
  tbcli.parse_line_buffer("show config", true);

  ASSERT_EQ (1, tbcli.lvl_1_);

  // load config
  ParseNodeFunctional *load = new ParseNodeFunctional (tbcli, "load", tbcli.root_parse_node_.get());
  ParseNodeFunctional *cli = new ParseNodeFunctional (tbcli, "cli", load);
  ParseNodeFunctional *config = new ParseNodeFunctional (tbcli, "config", load);
  ParseNodeValueInternal *name = new ParseNodeValueInternal (tbcli, RW_YANG_LEAF_TYPE_STRING, config);

  cli->set_callback( new CallbackTest (tbcli, &TestBaseCli::load_config));
  config->set_value_node (name);

  load->add_descendent(config);
  load->add_descendent(cli);


  tbcli.add_top_level_functional(load);
  tbcli.parse_line_buffer("load cli",true);

  ASSERT_EQ (1, tbcli.lvl_2_);

  tbcli.parse_line_buffer ("load config somefile", true);

}

TEST(YangCLIFunctional, CliPrintHookBasic) {
  TEST_DESCRIPTION("Test completions scenarios for config behavioral node");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ParseNodeFunctional* show_config_node =
      new ParseNodeFunctional (tbcli, "config", tbcli.show_node_.get());

  ASSERT_TRUE (!show_config_node->is_cli_print_hook());
  ASSERT_EQ (nullptr, show_config_node->get_cli_print_hook_string());

  show_config_node->set_cli_print_hook("Print_Hook");
  ASSERT_TRUE (show_config_node->is_cli_print_hook());
  ASSERT_EQ (0, strcmp ("Print_Hook", show_config_node->get_cli_print_hook_string()));

  show_config_node->set_cli_print_hook(0);

  ASSERT_TRUE (!show_config_node->is_cli_print_hook());
  ASSERT_EQ (nullptr, show_config_node->get_cli_print_hook_string());

}

TEST(YangCLIFunctional, CliPrintHookConfigShow) {
  TEST_DESCRIPTION("Test PrintHooks for show behavioral within config mode");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());

  const char* cmd1 = "show config";
  ParseLineResult p1(tbcli, cmd1, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p1.success_);
  EXPECT_STREQ(p1.cli_print_hook_string_, "test:config_print_hook");

  const char* cmd2 = "show config general-container";
  ParseLineResult p2(tbcli, cmd2, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p2.success_);
  EXPECT_STREQ(p2.cli_print_hook_string_, "test:config_print_hook");

  const char* cmd3 = "show general-container";
  ParseLineResult p3(tbcli, cmd3, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p3.success_);
  EXPECT_STREQ(p3.cli_print_hook_string_, "test:config_print_hook");

  ASSERT_TRUE(tbcli.exit_config_mode());

  const char* cmd4 = "show general-show";
  ParseLineResult p4(tbcli, cmd4, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p4.success_);
  EXPECT_EQ(p4.cli_print_hook_string_, nullptr);
}

TEST(YangCLIFunctional, CliPrintHookConfig) {
  TEST_DESCRIPTION("Test PrintHooks for config commands");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);

  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  tbcli.root_parse_node_->set_cli_print_hook("test:default_print");

  ASSERT_TRUE(tbcli.enter_config_mode());

  ParseLineResult r(tbcli, "general-container", ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(r.success_);
  EXPECT_STREQ(r.cli_print_hook_string_, "test:default_print");

  tbcli.mode_enter(&r);

  ParseLineResult r1(tbcli, "g-container gc-name test", ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(r1.success_);
  EXPECT_STREQ(r1.cli_print_hook_string_, "test:default_print");

  tbcli.mode_exit();

  ASSERT_TRUE(tbcli.exit_config_mode());
}

TEST (YangCLICompletion, LeafListInteger)
{
  TEST_DESCRIPTION("Test a leaf integer within a Container shows correctly");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");


  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());

  // A list of strings that all should have the same disposition
  const char  *endings[] = {"gc-l", "gc-leaf-l", "gc-leaf-list-unit8"};


  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {
    ParseLineResult r (tbcli, std::string("general-container g-container ") + std::string(endings[i]), ParseLineResult::NO_OPTIONS );

    EXPECT_EQ (r.success_, true);
    EXPECT_EQ (r.completions_.size(), 1);
    EXPECT_EQ (r.completions_[0].node_->get_stmt_type(), RW_YANG_STMT_TYPE_LEAF_LIST);
    EXPECT_EQ (r.completions_[0].node_->is_key(), false);
    EXPECT_TRUE (r.completions_[0].node_->is_leafy());
    EXPECT_TRUE (r.completions_[0].node_->is_config());
    EXPECT_TRUE (r.completions_[0].node_->is_keyword());

    EXPECT_EQ (r.completions_[0].node_->is_sentence(), false);
    EXPECT_EQ (r.completions_[0].node_->has_keys(), false);
  }
}

TEST (YangCLICompletion, LeafString)
{
  TEST_DESCRIPTION("Test a leaf integer within a Container shows correctly");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);

  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());
  tbcli.set_and_enter_mode("general-container", "goo");
  // A list of strings that all should have the same disposition
  const char  *endings[] = {"gc-n", "gc-na", "gc-name"};
  // ATTN: Reaching in here is wrong - should have accessors.
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);
  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {

    ParseLineResult r (tbcli, std::string("g-container ") +
                           std::string(endings[i]),
                         ParseLineResult::NO_OPTIONS );

    EXPECT_EQ (r.success_, true);
    EXPECT_EQ (r.completions_.size(), 1);
    EXPECT_EQ (r.completions_[0].node_->get_stmt_type(), RW_YANG_STMT_TYPE_LEAF);
    EXPECT_EQ (r.completions_[0].node_->is_key(), false);
    EXPECT_TRUE (r.completions_[0].node_->is_leafy());
    EXPECT_TRUE (r.completions_[0].node_->is_config());
    EXPECT_TRUE (r.completions_[0].node_->is_keyword());

    EXPECT_EQ (r.completions_[0].node_->is_sentence(), false);
    EXPECT_EQ (r.completions_[0].node_->has_keys(), false);
  }
}

TEST (YangCLICompletion, Container)
{
  TEST_DESCRIPTION("Test a leaf integer within a Container shows correctly");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");


  TestBaseCli tbcli(*model,0);

  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());
  tbcli.set_and_enter_mode("general-container", "goo");

  // A list of strings that all should have the same disposition
  const char  *endings[] = {"gc-s", "gc-sub", "gc-sub-container"};
  // ATTN: Reaching in here is wrong - should have accessors.
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);
  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {
    ParseLineResult r (tbcli, std::string("g-container ") + std::string(endings[i]), ParseLineResult::NO_OPTIONS );
    EXPECT_EQ (r.success_, true);
    EXPECT_EQ (r.completions_.size(), 1);
    EXPECT_EQ (r.completions_[0].node_->get_stmt_type(), RW_YANG_STMT_TYPE_CONTAINER);
    EXPECT_EQ (r.completions_[0].node_->is_key(), false);
    EXPECT_TRUE (!r.completions_[0].node_->is_leafy());
    EXPECT_TRUE (r.completions_[0].node_->is_config());
    EXPECT_TRUE (r.completions_[0].node_->is_keyword());

    EXPECT_EQ (r.completions_[0].node_->is_sentence(), false);
    EXPECT_EQ (r.completions_[0].node_->has_keys(), false);
  }
}

TEST (YangCLICompletion, Multiple)
{
  TEST_DESCRIPTION("Test string with multiple completions");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  ASSERT_TRUE(tbcli.enter_config_mode());
  tbcli.set_and_enter_mode("general-container", "goo");


  // A list of strings that all should have the same disposition
  const char  *endings[] = {"", "gc-", "int", "gc-sub-container"};
  const int counts[] = {6, 3, 2, 1};
  const int ecounts[] = {1, 0, 0, 1};
  const int ccounts[] = {2, 1, 1, 1};
  // ATTN: Reaching in here is wrong - should have accessors.
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);

  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {
    tbcli.generate_help(std::string("g-container") + std::string(endings[i]));
    tbcli.tab_complete(std::string("g-container") + std::string(endings[i]));

    std::vector<ParseMatch> matches = tbcli.generate_matches(
                      std::string("g-container") + std::string(endings[i]));
    EXPECT_EQ (matches.size(), ccounts[i]);

    ParseLineResult r (tbcli, std::string("g-container ") + std::string(endings[i]), ParseLineResult::NO_OPTIONS );
    EXPECT_EQ (r.success_, true);
    EXPECT_EQ (r.completions_.size(), counts[i]);

    ParseLineResult q (tbcli, std::string("g-container ") + std::string(endings[i]), ParseLineResult::ENTERKEY_MODE);
    EXPECT_EQ (q.completions_[0].node_->is_sentence(), false);
    EXPECT_EQ (q.completions_.size(), ecounts[i]);
    if ((i == 0)  || (i == 3)) {
      // completed keywords - parsing would work. sentence will not be complete though
      EXPECT_EQ (q.success_, true);
    }
    else {
      EXPECT_EQ (q.success_, false);
    }

  }
}

TEST (YangCLICompletion, SiblingCompletion)
{
  TEST_DESCRIPTION("Test string with multiple completions");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");


  TestBaseCli tbcli(*model,0);
  ASSERT_TRUE(tbcli.enter_config_mode());
  tbcli.set_and_enter_mode("general-container", "goo");

  // A list of strings that all should have the same disposition
  const char  *endings[] = {"gc-name a ", "int32-leaf 4 ", "gc-sub-container gc-sub-cont-int 45 "};

  // ATTN: Reaching in here is wrong - should have accessors.
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);

  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {
    ParseLineResult r (tbcli, std::string("g-container ") + std::string(endings[i]), ParseLineResult::ENTERKEY_MODE );
    EXPECT_EQ (r.success_, true);
  }
}

TEST (YangCLICompletion, List) {
  TEST_DESCRIPTION("Test the way lists complete");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  ASSERT_TRUE(tbcli.enter_config_mode());
  tbcli.set_and_enter_mode("general-container", "goo");

    // ATTN: Reaching in here is wrong - should have accessors.
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);

  // A list of strings that all should have the same disposition
  const char  *endings[] = {" "};
  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {
    tbcli.generate_help(std::string("g-list") + std::string(endings[i]));
    tbcli.tab_complete(std::string("g-list") + std::string(endings[i]));
    ParseLineResult r (tbcli, std::string("g-list") + std::string(endings[i]), ParseLineResult::NO_OPTIONS );

    ASSERT_TRUE (r.success_);
    EXPECT_EQ (2, r.completions_.size());

    EXPECT_TRUE (r.completions_[0].node_->is_key());
    EXPECT_TRUE (r.completions_[0].node_->is_leafy());
    EXPECT_TRUE (r.completions_[0].node_->is_config());

    EXPECT_TRUE (!r.completions_[0].node_->is_sentence());
    EXPECT_TRUE (!r.completions_[0].node_->has_keys());
  }

  const char  *endings2[] = {"2"};
  for (size_t i = 0; i < sizeof (endings2)/sizeof (char *); i++) {
    ParseLineResult r (tbcli, std::string("g-list ") + std::string(endings2[i]), ParseLineResult::NO_OPTIONS );
    EXPECT_EQ (true, r.success_);
    EXPECT_EQ (1, r.completions_.size());
    EXPECT_TRUE (r.completions_[0].node_->is_leafy());
    EXPECT_TRUE (r.completions_[0].node_->is_config());


    EXPECT_EQ (r.completions_[0].node_->is_sentence(), false);
    EXPECT_EQ (r.completions_[0].node_->has_keys(), false);
  }


  const char  *endings3[] = {"2 "};
  for (size_t i = 0; i < sizeof (endings3)/sizeof (char *); i++) {
    ParseLineResult r (tbcli, std::string("g-list ") + std::string(endings3[i]), ParseLineResult::NO_OPTIONS );
    EXPECT_EQ (true, r.success_);

  }
}

TEST (YangCLICompletion, LeafEnum)
{
  TEST_DESCRIPTION("Test a leaf integer within a Container shows correctly");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  ASSERT_TRUE(tbcli.enter_config_mode());
  tbcli.set_and_enter_mode("general-container", "goo");

  ParseLineResult r (tbcli, std::string("g-container double-key 23 name "), ParseLineResult::NO_OPTIONS );

  EXPECT_EQ (r.success_, true);
  ASSERT_EQ (3, r.completions_.size());

  for (size_t i = 0; i < 3; i++) {
    EXPECT_TRUE (r.completions_[i].node_->is_leafy());
    EXPECT_TRUE (r.completions_[i].node_->is_config());
    EXPECT_TRUE (r.completions_[i].node_->is_keyword());

    EXPECT_EQ (r.completions_[i].node_->is_sentence(), false);
    EXPECT_EQ (r.completions_[i].node_->has_keys(), false);
  }

  const char  *endings[] = {"f", "se", "sev", "seventh"};
  const int counts[] = {1, 2, 1, 1};


  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {

    ParseLineResult r (tbcli, std::string("g-container double-key 23 name ") + std::string(endings[i]), ParseLineResult::NO_OPTIONS );

    EXPECT_EQ (r.success_, true);
    EXPECT_EQ (r.completions_.size(), counts[i]);
  }
}

TEST (YangCLIModes, ModeAtList)
{
  TEST_DESCRIPTION("Test string with multiple completions");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  XMLNode *before, *after,*empty = nullptr,*top = nullptr, *middle = nullptr, *bottom = nullptr;
  XMLNodeList::uptr_t children;
  ModeState *config_mode = 0;
  // A list of strings that all should have the same disposition
  // ATTN: Reaching in here is wrong - should have accessors.
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);

  UNUSED (top);
  UNUSED (middle);
  UNUSED (bottom);

  ASSERT_TRUE(tbcli.enter_config_mode());

  ParseLineResult r (tbcli, std::string("general-container g-list 4 "), ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE (r.success_);
  ASSERT_TRUE (r.parse_tree_->is_sentence());

  before = tbcli.current_mode_->dom_node_;
  top = before;
  config_mode = tbcli.current_mode_;
  tbcli.mode_enter (&r);
  children = std::move(before->get_children());

  // Should have one child - the list
  EXPECT_EQ (1, children->length());
  ASSERT_EQ (std::string("general-container"),children->at(0)->get_local_name());
  before = children->at(0);

  children = std::move(before->get_children());
  // Should have one child - the list
  EXPECT_EQ (1, children->length());
  ASSERT_EQ (std::string("g-list"),children->at(0)->get_local_name());
  before = children->at(0);

  after = before->find_value("index", "4");

  ASSERT_NE (after, empty);
  ASSERT_EQ(before, tbcli.current_mode_->dom_node_);
  middle = before;
  // add a new mode using function calls
  ParseLineResult q (tbcli, "gcl-container", ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE (q.success_);
  ASSERT_TRUE (!q.result_mode_top_);
  ASSERT_TRUE (!q.parse_tree_->is_sentence());

  q.result_node_->set_mode ("glc");

  // enter that mode now.
  ParseLineResult s(tbcli, " gcl-container", ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE (s.success_);
  ASSERT_TRUE (s.result_mode_top_);
  ASSERT_TRUE (s.parse_tree_->is_sentence());

  tbcli.mode_enter (&s);

  // Should have 2 children - the index at 4
  EXPECT_EQ (2, before->get_children()->length());
  before = before->find("gcl-container");
  ASSERT_EQ(before, tbcli.current_mode_->dom_node_);
  bottom = before;

  const char *endings[] = {"having_a_bool false", "gclc-empty", "gclc-bits first"};

  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {
    ParseLineResult t(tbcli, endings[i], ParseLineResult::ENTERKEY_MODE);
    ASSERT_TRUE (t.success_);

    tbcli.current_mode_->update_xml_cfg (tbcli.config_.get(), t.result_tree_);
    char *nodename = strdup (endings[i]);
    char *space = strchr(nodename, ' ');
    if (space) {
      *space = 0;
      space ++;
    }
    if (space) {
      after = before->find_value(nodename, space);
    } else {
      after = before->find(nodename);
    }
    ASSERT_NE (after, empty);
  }

  // Ensure that overwrite of leaf do not create more children
  EXPECT_EQ (3, before->get_children()->length());

  const char *endings2[] = {"having_a_bool true", "gclc-empty", "gclc-bits second"};

  for (size_t i = 0; i < sizeof (endings)/sizeof (char *); i++) {
    ParseLineResult t(tbcli, endings2[i], ParseLineResult::ENTERKEY_MODE);
    ASSERT_TRUE (t.success_);

    char *nodename = strdup (endings[i]);
    char *space = strchr(nodename, ' ');

    tbcli.current_mode_->update_xml_cfg (tbcli.config_.get(), t.result_tree_);
    // ensure that the previous entry is gone
    if (i != 1) { // empty will remain the same
      if (space) {
        *space = 0;
        space ++;
      }
      if (space) {
        after = before->find_value(nodename, space);
      } else {
        after = before->find( nodename);
      }
      ASSERT_EQ (after, empty);
    }

     nodename = strdup (endings2[i]);
     space = strchr(nodename, ' ');
     if (space) {
       *space = 0;
       space ++;
     }

     if (space) {
       after = before->find_value(nodename, space);
     } else {
       after = before->find(nodename);
     }

     children = std::move(before->get_children());
     EXPECT_EQ (3, children->length());
     ASSERT_NE (after, empty);
  }

  ParseLineResult u(tbcli, "exit", ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE (u.success_);
  EXPECT_EQ (1, u.completions_.size());
  EXPECT_TRUE (u.completions_[0].node_->is_built_in());

  tbcli.mode_exit();
  ASSERT_EQ (middle, tbcli.current_mode_->dom_node_);

  // going into the container again - should get to the same mode
  // same result as was stored earlier..
  ParseLineResult w(tbcli, "gcl-container", ParseLineResult::ENTERKEY_MODE);
  tbcli.mode_enter (&w);
  // Should have 2 children - the index at 4
  ASSERT_EQ(bottom, tbcli.current_mode_->dom_node_);

  // all the way to the top now.
  tbcli.mode_pop_to(config_mode);
  ASSERT_EQ(top, tbcli.current_mode_->dom_node_);

  // Adding a new list should go to a different xml..
  ParseLineResult v (tbcli, std::string("general-container g-list 5 "), ParseLineResult::ENTERKEY_MODE);

  tbcli.mode_enter (&v);

  before =  tbcli.current_mode_->dom_node_;
  ASSERT_NE (middle, before);

  // generate xml corresponding to one string

  // ATTN - these functions should be tested better
  tbcli.generate_help ("end");
  tbcli.tab_complete ("end");

  tbcli.generate_help ("xxxxx");
  tbcli.tab_complete ("xxxx");


  tbcli.mode_end_even_if_root();

  // go to the top
  ASSERT_TRUE(tbcli.enter_config_mode());
  ParseLineResult x (tbcli, "general-container g-container gc-leaf-list-unit8 200", ParseLineResult::ENTERKEY_MODE );
  ASSERT_TRUE (x.success_);
}


TEST (YangCliPrintHook, cliPrintHook)
{
  TEST_DESCRIPTION("Test the cli print hook extension");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("testncx-cli");

  TestBaseCli tbcli(*model,0);
  // A list of strings that all should have the same disposition
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);

  ParseLineResult r (tbcli, std::string("top show port counters all"), ParseLineResult::NO_OPTIONS );

  ASSERT_THAT(r.cli_print_hook_string_, testing::StrEq("test-cli-print-hook"));
  EXPECT_EQ (r.success_, true);
}

TEST (YangCliPrintHook, cliPrintHookAppData)
{
  TEST_DESCRIPTION("Test the cli print hook extension");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("testncx-cli");

  TestBaseCli tbcli(*model,0);

  typedef AppDataParseToken<const char*> adpt_t;
  adpt_t adpt0 = adpt_t::create_and_register("http://riftio.com/ns/riftware-1.0/rw-cli-ext", "cli-print-hook", &tbcli);
  EXPECT_EQ(adpt0.app_data_index,1);

  // A list of strings that all should have the same disposition
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);

  ParseLineResult r (tbcli, std::string("top show port counters all"), ParseLineResult::NO_OPTIONS );

  ParseNode *nodep;
  EXPECT_TRUE(r.app_data_is_cached_last(adpt0, &nodep));

  const char* lookup = nullptr;
  EXPECT_TRUE(adpt0.parse_node_check_and_get(nodep, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_STREQ(lookup, "test-cli-print-hook");
  EXPECT_EQ (r.success_, true);
}

// This test was written before the introduction of behavioral nodes.
// This is now disblled since the base CLI now has only show/config and no,
// and commands like show config are implemented in rw_cli by enhancing
// behaviorals.

TEST (YangCLIApi, DISABLED_ParseArgvOne)
{
  TEST_DESCRIPTION("Test parse_argv_one");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  static const char* const cmd[] = { "show", "config", "text" };
  EXPECT_TRUE(tbcli.parse_argv_one(3, cmd));
}

// This test was written before the introduction of behavioral nodes.
// This is now disblled since the base CLI now has only show/config and no,
// and commands like show config are implemented in rw_cli by enhancing
// behaviorals.

TEST (YangCLIApi, DISABLED_ParseArgvSet)
{
  TEST_DESCRIPTION("Test parse_argv_set");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.root_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);
  tbcli.root_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);

  static const char* const cmds[] = {
    "show config text",
    "show config fooxml",
    //"config g-list index 5",
    "config g-list 5",
    "gcl-container gclc-bits first",
  };
  EXPECT_TRUE(tbcli.parse_argv_set(4, cmds));
  EXPECT_EQ(tbcli.root_mode_,tbcli.current_mode_);
}


TEST (YangCLICompletion, DISABLED_FailingTests) {

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");


  TestBaseCli tbcli(*model,0);
  // A list of strings that all should have the same disposition
  ParseLineResult r (tbcli, std::string("config g-li "), ParseLineResult::NO_OPTIONS );

  EXPECT_EQ (true, r.success_);
  EXPECT_EQ (1, r.completions_.size());

  EXPECT_TRUE (r.completions_[0].node_->has_keys());
}

TEST (CliAppData, Create)
{
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("testncx-cli-app-data");

  TestBaseCli tbcli(*model,0);

  // BaseCLI already registers for mode_string, print_hook, show-key-keyword extensions
  typedef AppDataParseToken<char*> adpt_t;
  adpt_t adpt0 = adpt_t::create_and_register("mod","ext0",&tbcli);
  EXPECT_EQ(adpt0.app_data_index,4);

  adpt_t adpt1 = adpt_t::create_and_register("mod","ext1",&tbcli);
  EXPECT_EQ(adpt1.app_data_index,5);
}


TEST (CliAppData, ParserLookup)
{
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("testncx-cli-app-data");
  const char* ns = "http://riftio.com/ns/core/util/yangtools/tests/testncx-cli-app-data";
  typedef AppDataToken<const char*> adt_t;

  TestBaseCli tbcli(*model,0);

  // BaseCLI already registers for mode_string, print_hook, show-key-keyword extensions
  typedef AppDataParseToken<const char*> adpt_t;
  adpt_t adpt1 = adpt_t::create_and_register(ns,"ext1",&tbcli);
  EXPECT_EQ(adpt1.app_data_index,4);

  adpt_t adpt2 = adpt_t::create_and_register(ns,"ext2",&tbcli);
  EXPECT_EQ(adpt2.app_data_index,5);

  // test duplicates
  adpt_t adpt3 = adpt_t::create_and_register(ns,"ext1",&tbcli);
  EXPECT_EQ(adpt3.app_data_index,4);

  adt_t adt1;
  EXPECT_TRUE(model->app_data_get_token(ns, "ext1", &adt1));
  EXPECT_EQ(adt1.index_, 4);
  EXPECT_STREQ(adt1.ns_.c_str(), ns);
  EXPECT_STREQ(adt1.ext_.c_str(), "ext1");
  EXPECT_TRUE(adt1.deleter_);

  ParseLineResult r(tbcli, std::string("top "), ParseLineResult::NO_OPTIONS);

  EXPECT_EQ(true, r.success_);
  EXPECT_EQ(2, r.completions_.size());

  EXPECT_TRUE(adpt1.parse_node_is_cached(r.result_node_));
  EXPECT_FALSE(adpt2.parse_node_is_cached(r.result_node_));
  EXPECT_TRUE(adpt3.parse_node_is_cached(r.result_node_));

  const char* lookup = nullptr;
  EXPECT_TRUE(adpt1.parse_node_check_and_get(r.result_node_, &lookup));
  ASSERT_TRUE(lookup);
  EXPECT_STREQ(lookup, "ext1 in top");
}


TEST (LineParser, Parse)
{
  const char *test_str[] = {"some string", " some string ", " some  string  "};
  const char *quoted_str[] = {"\"some\" string", "\"some and \" another",
                              "\" a quote \\\" within \" a quoted"};
  const char *failed_str[] = {"\"\"", "\" blargh"};
  std::vector<std::string> arr;

  for (size_t i = 0; i < sizeof (test_str)/sizeof (char *); i++)
  {
    std::string test = test_str[i];
    arr.clear();
    rw_cli_string_parse_status_t ret = parse_input_line (test, arr);

    ASSERT_EQ (RW_CLI_STRING_PARSE_STATUS_OK, ret);
    ASSERT_EQ (2, arr.size());
    EXPECT_STREQ ("some", arr[0].c_str());
    EXPECT_STREQ ("string", arr[1].c_str());
  }

  for (size_t i = 0; i < sizeof (quoted_str)/sizeof (char *); i++)
  {
    std::string test = quoted_str[i];
    arr.clear();
    rw_cli_string_parse_status_t ret = parse_input_line (test, arr);

    ASSERT_EQ (RW_CLI_STRING_PARSE_STATUS_OK, ret);


    switch (i) {
      case 0:
        ASSERT_EQ (2, arr.size());
        EXPECT_STREQ("some", arr[0].c_str());
        EXPECT_STREQ("string", arr[1].c_str());
        continue;

      case 1:
        ASSERT_EQ (2, arr.size());
        EXPECT_STREQ("some and ", arr[0].c_str());
        EXPECT_STREQ("another", arr[1].c_str());
        continue;

      case 2:
        ASSERT_EQ (3, arr.size());
        EXPECT_STREQ(" a quote \\\" within ", arr[0].c_str());
        EXPECT_STREQ("a", arr[1].c_str());
        EXPECT_STREQ("quoted", arr[2].c_str());
        continue;
    }
  }

  for (size_t i = 0; i < sizeof (failed_str)/sizeof (char *); i++)
  {
    std::string test = failed_str[i];
    arr.clear();
    rw_cli_string_parse_status_t ret = parse_input_line (test, arr);

    switch (i) {
      case 0:
        ASSERT_EQ (ret, RW_CLI_STRING_PARSE_STATUS_BLANK_WORD_IN_QUOTES);
        continue;

      case 1:
        ASSERT_EQ (ret, RW_CLI_STRING_PARSE_STATUS_QUOTES_NOT_CLOSED);
        continue;
    }
  }
}

TEST (YangCLI, NoNotif)
{
  TEST_DESCRIPTION("Test notifications missing");
  YangModelNcx* model = YangModelNcx::create_model();
  ASSERT_TRUE(model);
  YangModel::ptr_t p(model);
  YangModule* mod = model->load_module("yang-cli-test");
  ASSERT_TRUE(mod);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);
  YangNode* yn = root->search_child("you-cant-see-me-in-cli");
  ASSERT_TRUE(yn);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ParseLineResult r (tbcli, yn->get_name(), ParseLineResult::ParseLineResult::ENTERKEY_MODE);
  EXPECT_FALSE(r.success_);
}

TEST (YangCLIEmpty, EmptyCompletion)
{
  TEST_DESCRIPTION("Test RPC node for missing mandatory fields");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);

  ASSERT_TRUE(tbcli.enter_config_mode());

  ParseLineResult r (tbcli, std::string("general-container g-list index 4"), ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE (r.success_);
  ASSERT_TRUE (r.parse_tree_->is_sentence());
  tbcli.mode_enter (&r);

  const char* emptyBase = "gcl-container gclc-empty ";

  ParseLineResult a(tbcli, emptyBase, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(a.success_);
  // The other completions should be 'gclc-bits' and 'having_a_bool'
  EXPECT_EQ(2, a.completions_.size());

  ParseLineResult b(tbcli, emptyBase, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(b.success_);
  ASSERT_TRUE(b.parse_tree_->is_sentence());

  const char* endings[] = { "gclc-bits second", "having_a_bool true" };
  for (size_t i = 0; i < sizeof(endings)/sizeof(char*); i++)
  {
    ParseLineResult p(tbcli, std::string(emptyBase) + std::string(endings[i]),
                      ParseLineResult::ENTERKEY_MODE);
    ASSERT_TRUE(p.success_);
    ASSERT_TRUE(p.parse_tree_->is_sentence());
  }

  ASSERT_TRUE(tbcli.exit_config_mode(false));
}

TEST (YangCLIMandatory, MandatoryCheck)
{
  TEST_DESCRIPTION("Test empty yang node completion with siblings");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  const char* avengers_base = "avengers cast iron-man";

  std::vector<std::string> a_missing;
  ParseLineResult a(tbcli, avengers_base, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(a.success_);
  ASSERT_TRUE(a.parse_node_->is_rpc_input());
  a_missing = a.parse_tree_->check_for_mandatory();
  ASSERT_TRUE(a_missing.size() == 1);
  EXPECT_TRUE(a_missing[0] == std::string("type")); 

  const char* avengers_actor = " actor robert-downey-junior";
  std::vector<std::string> b_missing;
  ParseLineResult b(tbcli, std::string(avengers_base) + std::string(avengers_actor),
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(b.success_);
  ASSERT_TRUE(b.parse_node_->is_rpc_input());
  b_missing = b.parse_tree_->check_for_mandatory();
  ASSERT_TRUE(b_missing.size() == 1);
  EXPECT_TRUE(b_missing[0] == std::string("type")); 

  const char* avengers_type = " type hyper_hero";
  std::vector<std::string> c_missing;
  ParseLineResult c(tbcli, std::string(avengers_base) + std::string(avengers_type)
                    + std::string(avengers_actor),
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(c.success_);
  ASSERT_TRUE(c.parse_node_->is_rpc_input());
  c_missing = c.parse_tree_->check_for_mandatory();
  ASSERT_TRUE(c_missing.empty());

  // checking for child mandatory
  const char* abilities = " abilities vehicle suite";
  std::vector<std::string> d_missing;
  ParseLineResult d(tbcli, std::string(avengers_base) + std::string(avengers_type)
                    + std::string(abilities),
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(d.success_);
  ASSERT_TRUE(d.parse_node_->is_rpc_input());
  d_missing = d.parse_tree_->check_for_mandatory();
  ASSERT_TRUE(d_missing.size() == 2);
  EXPECT_TRUE(d_missing[0] == std::string("power"));
  EXPECT_TRUE(d_missing[1] == std::string("weapon"));

}

TEST (YangCLILeafList, LeafListCheck)
{
  TEST_DESCRIPTION("Test leaf list to get sibling nodes too");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());

  const char* leaflist_base = "ip-addrs ip 1.1.1.1 ";
  ParseLineResult a(tbcli, leaflist_base, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(a.success_);
  EXPECT_EQ(a.completions_.size(), 2);

  const char* leaflist_a1 = "ip-addrs port 1234 ip 1.1.1.1 ";
  ParseLineResult a1(tbcli, leaflist_a1, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(a1.success_);
  EXPECT_EQ(a1.completions_.size(), 1);

  const char* leaflist_a2 = "ip-addrs ip ";
  ParseLineResult a2(tbcli, leaflist_a2, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(a2.success_);
  EXPECT_EQ(a2.completions_.size(), 1);

  const char* leaflist_b = "ip-addrs ip 1.1.1.1 2.2.2.2";
  ParseLineResult b(tbcli, leaflist_b, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(b.success_);
  ASSERT_TRUE(b.parse_tree_->is_sentence());
  {
    XMLDocument::uptr_t dom(tbcli.get_command_dom(b.result_tree_->children_.begin()->get()));
    XMLNode* root = dom->get_root_node();
    XMLNode* fchild = root->get_first_child();
    ASSERT_EQ (2, fchild->get_children()->length());
    ASSERT_STREQ(fchild->get_local_name().c_str(), "ip-addrs");
    ASSERT_STREQ(fchild->get_children()->at(0)->get_local_name().c_str(), "ip");
    ASSERT_STREQ(fchild->get_children()->at(0)->get_text_value().c_str(), "1.1.1.1");
    ASSERT_STREQ(fchild->get_children()->at(1)->get_local_name().c_str(), "ip");
    ASSERT_STREQ(fchild->get_children()->at(1)->get_text_value().c_str(), "2.2.2.2");
  }

  const char* leaflist_c = "ip-addrs ip 1.1.1.1 2.2.2.2 port 1234";
  ParseLineResult c(tbcli, leaflist_c, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(c.success_);
  ASSERT_TRUE(c.parse_tree_->is_sentence());
  {
    XMLDocument::uptr_t dom(tbcli.get_command_dom(c.result_tree_->children_.begin()->get()));
    XMLNode* root = dom->get_root_node();
    XMLNode* fchild = root->get_first_child();
    ASSERT_EQ (3, fchild->get_children()->length());
    ASSERT_STREQ(fchild->get_local_name().c_str(), "ip-addrs");
    ASSERT_STREQ(fchild->get_children()->at(0)->get_local_name().c_str(), "ip");
    ASSERT_STREQ(fchild->get_children()->at(0)->get_text_value().c_str(), "1.1.1.1");
    ASSERT_STREQ(fchild->get_children()->at(1)->get_local_name().c_str(), "ip");
    ASSERT_STREQ(fchild->get_children()->at(1)->get_text_value().c_str(), "2.2.2.2");
    ASSERT_STREQ(fchild->get_children()->at(2)->get_local_name().c_str(), "port");
    ASSERT_STREQ(fchild->get_children()->at(2)->get_text_value().c_str(), "1234");
  }

  const char* leaflist_d = "ip-addrs ip 1.1.1.1 port 1234";
  ParseLineResult d(tbcli, leaflist_d, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(d.success_);
  ASSERT_TRUE(d.parse_tree_->is_sentence());
  {
    XMLDocument::uptr_t dom(tbcli.get_command_dom(d.result_tree_->children_.begin()->get()));
    XMLNode* root = dom->get_root_node();
    XMLNode* fchild = root->get_first_child();
    ASSERT_EQ (2, fchild->get_children()->length());
    ASSERT_STREQ(fchild->get_local_name().c_str(), "ip-addrs");
    ASSERT_STREQ(fchild->get_children()->at(0)->get_local_name().c_str(), "ip");
    ASSERT_STREQ(fchild->get_children()->at(0)->get_text_value().c_str(), "1.1.1.1");
    ASSERT_STREQ(fchild->get_children()->at(1)->get_local_name().c_str(), "port");
    ASSERT_STREQ(fchild->get_children()->at(1)->get_text_value().c_str(), "1234");
  }

  const char* leaflist_e = "ip-addrs port 1234 ip 1.1.1.1 2.2.2.2";
  ParseLineResult e(tbcli, leaflist_e, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(e.success_);
  ASSERT_TRUE(e.parse_tree_->is_sentence());
  {
    XMLDocument::uptr_t dom(tbcli.get_command_dom(e.result_tree_->children_.begin()->get()));
    XMLNode* root = dom->get_root_node();
    XMLNode* fchild = root->get_first_child();
    ASSERT_EQ (3, fchild->get_children()->length());
    ASSERT_STREQ(fchild->get_local_name().c_str(), "ip-addrs");
    ASSERT_STREQ(fchild->get_children()->at(0)->get_local_name().c_str(), "port");
    ASSERT_STREQ(fchild->get_children()->at(0)->get_text_value().c_str(), "1234");
    ASSERT_STREQ(fchild->get_children()->at(1)->get_local_name().c_str(), "ip");
    ASSERT_STREQ(fchild->get_children()->at(1)->get_text_value().c_str(), "1.1.1.1");
    ASSERT_STREQ(fchild->get_children()->at(2)->get_local_name().c_str(), "ip");
    ASSERT_STREQ(fchild->get_children()->at(2)->get_text_value().c_str(), "2.2.2.2");
  }

  ASSERT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIListMode, Config_KeyKeywordSupress)
{
  TEST_DESCRIPTION("Test Keyword suppression for keys in a list mode");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());

  std::string list_base("key-test ");
  tbcli.generate_help(list_base);
  tbcli.tab_complete(list_base);

  ParseLineResult a(tbcli, list_base, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(a.success_);
  ASSERT_EQ(2, a.completions_.size());

  std::string key1_val("key1val ");
  ParseLineResult b(tbcli, list_base + key1_val, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(b.success_);
  ASSERT_EQ(2, b.completions_.size());

  std::string key2_val("key2val ");
  ParseLineResult c(tbcli, list_base + key1_val + key2_val, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(c.success_);
  ASSERT_EQ(2, c.completions_.size());

  std::string key3_val("3 ");
  ParseLineResult d(tbcli, list_base + key1_val + key2_val + key3_val, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(d.success_);
  ASSERT_EQ(2, d.completions_.size());

  ParseLineResult e(tbcli, list_base + key1_val + key2_val + key3_val, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(e.success_);
  ASSERT_TRUE(e.parse_tree_->is_sentence());

  ParseLineResult f(tbcli, list_base + key1_val + key2_val + key2_val, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(!f.success_);

  ParseLineResult g(tbcli, list_base + std::string("key1 ") + key1_val + 
                    std::string("key2 ") + key2_val + 
                    std::string("key3 ") + key3_val, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(g.success_);
  ASSERT_TRUE(g.parse_tree_->is_sentence());

  ParseLineResult h(tbcli, list_base + std::string("key1 ") + key1_val, 
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(h.success_);
  ASSERT_TRUE(h.parse_tree_->is_sentence());

  ASSERT_TRUE(tbcli.exit_config_mode());
}


TEST(YangCLIListMode, No_Show_KeyKeywordSupress)
{
  TEST_DESCRIPTION("Test Keyword suppression for keys in a list mode");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  std::string list_base("key-test ");
  std::string key1_val("key1val ");
  std::string key2_val("key2val ");
  std::string key3_val("3 ");

  ParseLineResult a(tbcli, std::string("show ") + list_base +  key1_val + 
                key2_val + key3_val, 
                ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(a.success_);
  ASSERT_TRUE(a.parse_tree_->is_sentence());

  ASSERT_TRUE(tbcli.enter_config_mode());

  ParseLineResult b(tbcli, std::string("no ") + list_base +  key1_val + 
                key2_val + key3_val, 
                ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(b.success_);
  ASSERT_TRUE(b.parse_tree_->is_sentence());

  ParseLineResult c(tbcli, std::string("no ") + list_base +  
                std::string("key1 ") + key1_val + 
                std::string("key2 ") + key2_val + 
                std::string("key3 ") + key3_val, 
                ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(c.success_);
  ASSERT_TRUE(c.parse_tree_->is_sentence());

  ASSERT_TRUE(tbcli.exit_config_mode());

  ParseLineResult d(tbcli, std::string("show ") + list_base +  
                std::string("key1 ") + key1_val + 
                std::string("key2 ") + key2_val + 
                std::string("key3 ") + key3_val, 
                ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(d.success_);
  ASSERT_TRUE(d.parse_tree_->is_sentence());

  std::string key_vals("key1val key2val 3 ");
  tbcli.tab_complete(std::string("show ") + list_base + key_vals + "non-ke");

  ParseLineResult e(tbcli, std::string("show ") + list_base + key_vals + "non-ke", 
                    ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(e.success_);
  EXPECT_EQ(2, e.completions_.size());

  std::string non_keys("non-key1");
  ParseLineResult f(tbcli, std::string("show ") + list_base + key_vals + non_keys, 
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(f.success_);
  EXPECT_TRUE(f.parse_tree_->is_sentence());

  std::string child_list("list-nkey super counters");
  ParseLineResult g(tbcli, std::string("show ") + list_base + key_vals + child_list, 
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(g.success_);
  EXPECT_TRUE(g.parse_tree_->is_sentence());
}


TEST(YangCLIListMode, KeyKeywordExt)
{
  TEST_DESCRIPTION("Test Keyword suppression for keys in a list mode");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());

  std::string list_base("key-test-ext ");
  tbcli.generate_help(list_base);
  tbcli.tab_complete(list_base);

  ParseLineResult a(tbcli, list_base, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(a.success_);
  ASSERT_EQ(1, a.completions_.size());

  std::string key1_val("key1val ");
  ParseLineResult b(tbcli, list_base + key1_val, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(!b.success_);

  std::string key1("key1 key1val ");
  ParseLineResult c(tbcli, list_base + key1, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(c.success_);
  ASSERT_EQ(2, c.completions_.size());

  ParseLineResult d(tbcli, list_base + key1, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(d.success_);
  ASSERT_TRUE(d.parse_tree_->is_sentence());

  ParseLineResult e(tbcli, std::string("no ") + list_base + key1, 
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(e.success_);
  ASSERT_TRUE(e.parse_tree_->is_sentence());

  ParseLineResult f(tbcli, std::string("no ") + list_base + key1_val, 
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(!f.success_);

  ASSERT_TRUE(tbcli.exit_config_mode());

  ParseLineResult g(tbcli, std::string("show ") + list_base + key1, 
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(g.success_);
  ASSERT_TRUE(g.parse_tree_->is_sentence());

  ParseLineResult h(tbcli, std::string("show ") + list_base + key1_val, 
                    ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(!h.success_);

}

TEST(YangCLIList, EnterKeyWithSpace)
{
  TEST_DESCRIPTION("Test Keyword suppression for keys in a list mode");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());

  std::string app_config("ip-application test-app");
  ParseLineResult mode_r(tbcli, app_config, ParseLineResult::ENTERKEY_MODE);
  tbcli.mode_enter (&mode_r);

  std::string ip_conf("ip-address 11.0.1.3 ip-proto tcp ");
  ParseLineResult a(tbcli, ip_conf, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(a.success_);
  ASSERT_EQ(1, a.completions_.size());

  tbcli.mode_exit();

  ASSERT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIRpc, CheckOrder)
{
  TEST_DESCRIPTION("Test the ordering of RPC elements");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  
  const char* rpc = "avengers shield agents 16 "
      "helicarrier jet-fighters 32 missiles 500 turbines 24 "
      "director nick-fury executive-director maria";
  ParseLineResult a(tbcli, rpc, ParseLineResult::ENTERKEY_MODE);
  EXPECT_EQ (tbcli.parse_line_buffer(rpc, true), PARSE_LINE_RESULT_SUCCESS);
  ASSERT_TRUE(a.success_);
  ASSERT_TRUE(a.parse_tree_->is_sentence());
  {
    const char* expected_xml = 
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:avengers xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:shield>\n"
"      <cli:director>nick-fury</cli:director>\n"
"      <cli:helicarrier>\n"
"        <cli:jet-fighters>32</cli:jet-fighters>\n"
"        <cli:turbines>24</cli:turbines>\n"
"        <cli:missiles>500</cli:missiles>\n"
"      </cli:helicarrier>\n"
"      <cli:executive-director>maria</cli:executive-director>\n"
"      <cli:agents>16</cli:agents>\n"
"    </cli:shield>\n"
"  </cli:avengers>\n\n"
"</data>";

    XMLDocument::uptr_t dom(tbcli.get_command_dom(a.result_tree_->children_.begin()->get()));
    XMLNode* root = dom->get_root_node();
    EXPECT_EQ(root->to_string_pretty(), expected_xml);
  }
}

TEST(YangCLIRpc, MultiList)
{
  TEST_DESCRIPTION("Test the ordering of RPC elements");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");
  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  
  std::string cmd = "avengers cast iron-man type super_hero cast captain-america type hyper_hero shield director Fury helicarrier turbines 2 agents 4 ";

  ParseLineResult r(tbcli, cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(r.success_);
  ASSERT_TRUE(r.parse_tree_->is_sentence());
 
  std::string expected_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:avengers xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:cast>\n"
"      <cli:name>iron-man</cli:name>\n"
"      <cli:type>super_hero</cli:type>\n"
"    </cli:cast>\n"
"    <cli:cast>\n"
"      <cli:name>captain-america</cli:name>\n"
"      <cli:type>hyper_hero</cli:type>\n"
"    </cli:cast>\n"
"    <cli:shield>\n"
"      <cli:director>Fury</cli:director>\n"
"      <cli:helicarrier>\n"
"        <cli:turbines>2</cli:turbines>\n"
"      </cli:helicarrier>\n"
"      <cli:agents>4</cli:agents>\n"
"    </cli:shield>\n"
"  </cli:avengers>\n\n"
"</data>";
 
  XMLDocument::uptr_t dom(tbcli.get_command_dom(r.result_tree_->children_.begin()->get()));
  XMLNode* root = dom->get_root_node();
  EXPECT_EQ(expected_xml, root->to_string_pretty());

  // For completions at different levels
  std::string cmd1 = "avengers shield director Fury helicarrier turbines 2 ";
  tbcli.generate_help(cmd1);

  std::string cmd2 = "avengers cast iron-man type super_hero cast captain-america type hyper_hero abilities power aa weapon pshield shield director Fury helicarrier turbines 2 agents 4";
  ParseLineResult r1(tbcli, cmd2, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(r1.success_);
  ASSERT_TRUE(r1.parse_tree_->is_sentence());

  std::string expected_xml1 =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:avengers xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:cast>\n"
"      <cli:name>iron-man</cli:name>\n"
"      <cli:type>super_hero</cli:type>\n"
"    </cli:cast>\n"
"    <cli:cast>\n"
"      <cli:name>captain-america</cli:name>\n"
"      <cli:type>hyper_hero</cli:type>\n"
"      <cli:abilities>\n"
"        <cli:power>aa</cli:power>\n"
"        <cli:weapon>pshield</cli:weapon>\n"
"      </cli:abilities>\n"
"    </cli:cast>\n"
"    <cli:shield>\n"
"      <cli:director>Fury</cli:director>\n"
"      <cli:helicarrier>\n"
"        <cli:turbines>2</cli:turbines>\n"
"      </cli:helicarrier>\n"
"      <cli:agents>4</cli:agents>\n"
"    </cli:shield>\n"
"  </cli:avengers>\n\n"
"</data>";

  XMLDocument::uptr_t dom1(tbcli.get_command_dom(r1.result_tree_->children_.begin()->get()));
  XMLNode* root1 = dom1->get_root_node();
  EXPECT_EQ(expected_xml1, root1->to_string_pretty());
}

TEST(YangCLIIdentity, IdRefXml)
{
  TEST_DESCRIPTION("Test IdRef XML generation");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("test-ydom-cli");
  ASSERT_TRUE(module);
  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());

  const char* cmd = "network 123 net-type ethernet company riftio";
  ParseLineResult a(tbcli, cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(a.success_);
  ASSERT_TRUE(a.parse_tree_->is_sentence());

  const char* expected_xml = 
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:network xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:network-id>123</cli:network-id>\n"
"    <cli:net-type>cli:ethernet</cli:net-type>\n"
"    <cli:company xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-cli\">t:riftio</cli:company>\n"
"  </cli:network>\n\n"
"</data>";
  XMLDocument::uptr_t dom(tbcli.get_command_dom(a.result_tree_->children_.begin()->get()));
  XMLNode* root = dom->get_root_node();
  EXPECT_EQ(root->to_string_pretty(), expected_xml);

  ASSERT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIXML, MergeXML)
{
  TEST_DESCRIPTION("Test XML generation from Result Tree using merge_xml");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());

  const char* cmd = "key-test first second 3";
  ParseLineResult a(tbcli, cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(a.success_);

  XMLDocument::uptr_t command(tbcli.xmlmgr_->create_document());
  XMLNode *parent_xml = command->get_root_node();
  ModeState mode_state(tbcli, *(a.result_tree_.get()), parent_xml);

  XMLNode* result_xml = mode_state.merge_xml(command.get(), parent_xml, 
      a.result_tree_->children_.front().get());
  ASSERT_TRUE(result_xml);

  const char* expected1 = 
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:key-test xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:key1>first</cli:key1>\n"
"    <cli:key2>second</cli:key2>\n"
"    <cli:key3>3</cli:key3>\n"
"  </cli:key-test>\n\n"
"</data>";
  EXPECT_EQ(parent_xml->to_string_pretty(), expected1);

  tbcli.mode_enter(&a);

  const char* child_cmd = "non-key1 val1 non-key2 10";
  ParseLineResult b(tbcli, child_cmd, ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE(b.success_);
  for (auto& child_node : b.result_tree_->children_) {
    XMLNode *r1_xml = mode_state.merge_xml(command.get(), result_xml,
                        child_node.get());
    ASSERT_TRUE(r1_xml);
  }

  const char* expected2 = 
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:key-test xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:key1>first</cli:key1>\n"
"    <cli:key2>second</cli:key2>\n"
"    <cli:key3>3</cli:key3>\n"
"    <cli:non-key1>val1</cli:non-key1>\n"
"    <cli:non-key2>10</cli:non-key2>\n"
"  </cli:key-test>\n\n"
"</data>";
  EXPECT_EQ(parent_xml->to_string_pretty(), expected2);
  
  tbcli.mode_exit();
  EXPECT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIXML, No_ListWithKey)
{
  TEST_DESCRIPTION("XML Generation for delete operations on a list with key");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());

  const char* cmd = "no network 31415";
  ParseLineResult a(tbcli, cmd, ParseLineResult::ENTERKEY_MODE);

  ASSERT_TRUE(a.success_);

  XMLDocument::uptr_t cmd_dom = std::move(tbcli.get_command_dom_from_result(
                                                  a.result_tree_.get()));
  XMLNode* root = cmd_dom->get_root_node();

  const char* expected_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:network xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"delete\">\n"
"    <cli:network-id>31415</cli:network-id>\n"
"  </cli:network>\n\n"
"</data>";

  EXPECT_EQ(root->to_string_pretty(), expected_xml);


  // ===== No for list with multiple keys ====
  const char* mkey_cmd = "no key-test key1val key2val 3";
  ParseLineResult b(tbcli, mkey_cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(b.success_);
  XMLDocument::uptr_t mkey_cmd_dom = std::move(tbcli.get_command_dom_from_result(
                                                  b.result_tree_.get()));
  XMLNode* mkey_root = mkey_cmd_dom->get_root_node();

  const char* expected_mkey_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:key-test xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"delete\">\n"
"    <cli:key1>key1val</cli:key1>\n"
"    <cli:key2>key2val</cli:key2>\n"
"    <cli:key3>3</cli:key3>\n"
"  </cli:key-test>\n\n"
"</data>";

  EXPECT_EQ(mkey_root->to_string_pretty(), expected_mkey_xml);


  EXPECT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIXML, No_Leaf)
{
  TEST_DESCRIPTION("Test XML generation for Leaf node deletion");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());

  const char* expected_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:key-test xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:key1>key1val</cli:key1>\n"
"    <cli:key2>key2val</cli:key2>\n"
"    <cli:key3>3</cli:key3>\n"
"    <cli:non-key1 xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"delete\"/>\n"
"  </cli:key-test>\n\n"
"</data>";

  // ==== Leaf node deletion within a list ====
  const char *l_cmd = "no key-test key1val key2val 3 non-key1";
  ParseLineResult l(tbcli, l_cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(l.success_);

  XMLDocument::uptr_t l_cmd_dom = std::move(tbcli.get_command_dom_from_result(
                                                  l.result_tree_.get()));
  XMLNode* l_root = l_cmd_dom->get_root_node();

  EXPECT_EQ(l_root->to_string_pretty(), expected_xml);

  // ==== No for leaf within a mode ====
  const char* m_cmd = "key-test key1val key2val 3";
  ParseLineResult a(tbcli, m_cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(a.success_);

  tbcli.mode_enter(&a);

  const char* m_no_cmd = "no non-key1";
  ParseLineResult b(tbcli, m_no_cmd, ParseLineResult::ENTERKEY_MODE);
  XMLDocument::uptr_t m_cmd_dom = std::move(tbcli.get_command_dom_from_result(
                                                  b.result_tree_.get()));
  XMLNode* m_root = m_cmd_dom->get_root_node();

  EXPECT_EQ(m_root->to_string_pretty(), expected_xml);

  tbcli.mode_exit();
  EXPECT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIXML, No_Container)
{
  TEST_DESCRIPTION("Test XML generation for container deletion");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());

  // ==== Basic container delete ====
  const char* cmd = "no ip-addrs";
  ParseLineResult plr(tbcli, cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(plr.success_);

  XMLDocument::uptr_t cmd_dom = std::move(tbcli.get_command_dom_from_result(
                                                  plr.result_tree_.get()));
  XMLNode* root = cmd_dom->get_root_node();

  const char* expected_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:ip-addrs xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"delete\"/>\n\n"
"</data>";
  EXPECT_EQ(root->to_string_pretty(), expected_xml);

  // ==== Container within a container ====
  const char* cc_cmd = "no general-container g-container";
  ParseLineResult cc(tbcli, cc_cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(cc.success_);

  XMLDocument::uptr_t cc_cmd_dom = std::move(tbcli.get_command_dom_from_result(
                                                  cc.result_tree_.get()));
  XMLNode* cc_root = cc_cmd_dom->get_root_node();

  const char* cc_expected_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:general-container xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:g-container xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"delete\"/>\n"
"  </cli:general-container>\n\n"
"</data>";
  EXPECT_EQ(cc_root->to_string_pretty(), cc_expected_xml);

  // ==== Delete container within a mode ====
  const char* cm_cmd = "general-container";
  ParseLineResult cm(tbcli, cm_cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(cm.success_);

  tbcli.mode_enter(&cm);

  const char* c_no_cmd = "no g-container";
  ParseLineResult c_no(tbcli, c_no_cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(c_no.success_);

  XMLDocument::uptr_t c_no_cmd_dom = std::move(tbcli.get_command_dom_from_result(
                                                  c_no.result_tree_.get()));
  XMLNode* c_no_root = c_no_cmd_dom->get_root_node();

  EXPECT_EQ(c_no_root->to_string_pretty(), cc_expected_xml);

  tbcli.mode_exit();
  EXPECT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIXML, No_LeafList)
{
  TEST_DESCRIPTION("Test XML generation for leaf-list element deletion");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();
  ASSERT_TRUE(tbcli.enter_config_mode());

  // ==== Delete all leafs  ====
  {
    const char* cmd = "no ip-addrs ip";
    ParseLineResult plr(tbcli, cmd, ParseLineResult::ENTERKEY_MODE);
    ASSERT_TRUE(plr.success_);

    XMLDocument::uptr_t cmd_dom = std::move(tbcli.get_command_dom_from_result(
          plr.result_tree_.get()));
    XMLNode* root = cmd_dom->get_root_node();

    const char* expected_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:ip-addrs xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:ip xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"delete\"/>\n"
"  </cli:ip-addrs>\n\n"
"</data>";
    EXPECT_EQ(root->to_string_pretty(), expected_xml);
  }

  // ==== Delete a particular leaf ===
  {
    const char* cmd = "no ip-addrs ip 3.3.3.3";
    ParseLineResult plr(tbcli, cmd, ParseLineResult::ENTERKEY_MODE);
    ASSERT_TRUE(plr.success_);

    XMLDocument::uptr_t cmd_dom = std::move(tbcli.get_command_dom_from_result(
          plr.result_tree_.get()));
    XMLNode* root = cmd_dom->get_root_node();

    const char* expected_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <cli:ip-addrs xmlns:cli=\"http://riftio.com/ns/yangtools/yang-cli-test\">\n"
"    <cli:ip xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"delete\">3.3.3.3</cli:ip>\n"
"  </cli:ip-addrs>\n\n"
"</data>";
    EXPECT_EQ(root->to_string_pretty(), expected_xml);
  }

  EXPECT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIBehavioral, Quoting)
{
  TEST_DESCRIPTION ("Test scenarios for which parser must put extra quotes");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  model->load_module("yang-cli-test");

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  // Check regular quoting
  // Test will fail since its not processed before parsing
  const char* cmd = "show manifest inventory component \"A B C\"";

  ParseLineResult a(tbcli, cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE (!a.success_);

  // This is how cli would send quoted string ('nick fury')
  // if it had sent in a flat string buffer
  const char* rpc = "avengers shield agents 16 "
    "helicarrier jet-fighters 32 missiles 500 turbines 24 "
    "director nick fury executive-director maria";
  EXPECT_NE (tbcli.parse_line_buffer(rpc, true), PARSE_LINE_RESULT_SUCCESS);

  const char* rpc2[] = {
    "avengers", "shield", "agents", "16",
    "helicarrier", "jet-fighters", "32", "missiles", "500",
    "turbines", "24", "director", "nick fury", "executive-director",
    "maria"
  };

  EXPECT_EQ (tbcli.process_line_buffer(15, rpc2, true), PARSE_LINE_RESULT_SUCCESS);

  // With newline and embedded quotes
  const char* rpc3[] = {
    "avengers", "shield", "agents", "16",
    "helicarrier", "jet-fighters", "32", "missiles", "500",
    "turbines", "24", "director", "nick\nfury\'s", "executive-director",
    "maria\"d souza"
  };

  EXPECT_EQ (tbcli.process_line_buffer(15, rpc3, true), PARSE_LINE_RESULT_SUCCESS);

  // With unprintable characters
  const char* rpc4[] = {
    "avengers", "shield", "agents", "16",
    "helicarrier", "jet-fighters", "32", "missiles", "500",
    "turbines", "24", "director", "佐藤 �", "executive-director",
    "佐藤 �"
  };

  EXPECT_EQ (tbcli.process_line_buffer(15, rpc4, true), PARSE_LINE_RESULT_SUCCESS);
}

TEST(YangCLINsPrefix, ManifestPath)
{
  TEST_DESCRIPTION("Test parsing commands with module prefix as done by CliManifest");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  module = model->load_module("other-rwcli_test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.root_parse_node_->flags_.set_inherit(ParseFlags::V_LIST_KEYS_CLONED);

  std::string network_cmd("network");
  ParseLineResult p1(tbcli, network_cmd, ParseLineResult::NO_OPTIONS);
  ASSERT_TRUE(p1.success_);
  ASSERT_EQ(0, p1.completions_.size());

  std::string clitest_network_cmd("rwclitest:network ");
  ParseLineResult p2(tbcli, clitest_network_cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p2.success_);
  EXPECT_EQ(p2.completions_.size(), 1);
  EXPECT_TRUE(p2.result_node_);

  std::string cli_network_cmd("cli:network ");
  ParseLineResult p3(tbcli, cli_network_cmd, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p3.success_);
  EXPECT_EQ(p3.completions_.size(), 1);
  EXPECT_TRUE(p3.result_node_);

  std::string path1 = clitest_network_cmd + std::string("servers ");
  ParseLineResult p4(tbcli, path1, ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p4.success_);
  EXPECT_TRUE(p4.result_node_);

  std::string path2 = cli_network_cmd + std::string("servers ");
  ParseLineResult p5(tbcli, path2, ParseLineResult::ENTERKEY_MODE);
  ASSERT_FALSE(p5.success_);
}

TEST(YangCLINsPrefix, HelpAndTabComplete)
{
  TEST_DESCRIPTION("Test Help and Tab complete when there prefix is required");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  module = model->load_module("other-rwcli_test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  ASSERT_TRUE(tbcli.enter_config_mode());
  
  tbcli.generate_help("rwcli");
  
  std::string line = tbcli.tab_complete("rwcli");
  EXPECT_EQ(line, "rwclitest:");

  std::vector<ParseMatch> matches = tbcli.generate_matches("rwcli");
  ASSERT_EQ (matches.size(), 2);
  EXPECT_EQ (matches[0].match_, "rwclitest:network");
  EXPECT_EQ (matches[1].match_, "rwclitest:show");

  line = tbcli.tab_complete("cli:");
  EXPECT_EQ(line, "cli:network ");

  matches = tbcli.generate_matches("cli:");
  ASSERT_EQ (matches.size(), 1);
  EXPECT_EQ (matches[0].match_, "cli:network");

  EXPECT_TRUE(tbcli.exit_config_mode());
}

TEST(YangCLIAppData, SuppressNamespace)
{
  TEST_DESCRIPTION("Test Help and Tab complete when there prefix is required");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  module = model->load_module("other-rwcli_test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.set_rwcli_like_params();

  // ATTN the namespace of other-rwcli_test should be a test namespace, this has
  // to be changed
  tbcli.suppress_namespace("urn:ietf:params:xml:ns:yang:rift:config");

  ParseLineResult p1(tbcli, "show port all", ParseLineResult::ENTERKEY_MODE);
  ASSERT_FALSE(p1.success_);

  ParseLineResult p2(tbcli, "show general-show g-container", ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p2.success_);
}

TEST(YangCLIAppData, SuppressCommand)
{
  TEST_DESCRIPTION("Test Help and Tab complete when there prefix is required");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  YangModule* module = nullptr;

  module = model->load_module("yang-cli-test");
  ASSERT_TRUE(module);

  TestBaseCli tbcli(*model,0);
  tbcli.root_parse_node_->flags_.set_inherit(ParseFlags::V_LIST_KEYS_CLONED);

  tbcli.suppress_command_path("general-container g-container");

  ParseLineResult p1(tbcli, "general-container g-container gc-name test", ParseLineResult::ENTERKEY_MODE);
  ASSERT_FALSE(p1.success_);

  ParseLineResult p2(tbcli, "general-container g-list index 1",  ParseLineResult::ENTERKEY_MODE);
  ASSERT_TRUE(p2.success_);
}
