/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file yangcli.cpp
 * @author Tom Seidenberg (tom.seidenberg@riftio.com)
 * @date 02/12/2014
 * @brief Base CLI definitions for Rift CLIs
 */

// This file was previously in the following path
//   modules/core/utils/yangtools/lib/yangcli.cpp
// For commit history refer the submodule modules/core/utils

#include <iomanip>
#include <stack>
#include <algorithm>
#include <iterator>
#include <sstream>
#include "rwlib.h"
#include "yangcli.hpp"

using namespace rw_yang;


namespace rw_yang {

static const char* NETCONF_NS = "urn:ietf:params:xml:ns:netconf:base:1.0";
static const char* NETCONF_NS_PREFIX = "xc";

static const char* RW_YANG_CLI_EXT_SUPPRESS_TOKEN = "suppress-keyword";


/*****************************************************************************/
/**
 * A predicate to check if the string needs to be put
 * inside quotes. The default case for which it is done
 * is when the passed word has a space(' ') or a tab ('\t')
 * present in it.
 */
bool is_to_be_cooked(const std::string& word)
{
  if (word.find_first_of(" \t\r\n\v\f\"'") == std::string::npos) {
    return false;
  }
  return true;
}

bool escape_quotes(std::string& str)
{
  bool ret = false;
  size_t pos = 0;
  size_t start = 0;

  while ((pos = str.find_first_of("\"\\", start)) != std::string::npos) {
    str.insert(pos, "\\");
    start = pos + 2;
    ret = true;
  }

  return ret;
}

bool ParseFlags::is_good(flag_t v)
{
  return (int)v >= (int)FIRST && (int)v <= (int)LAST;
}

bool ParseFlags::is_good_or_none(flag_t v)
{
  return v == NONE || ((int)v >= (int)FIRST && (int)v <= (int)LAST);
}

size_t ParseFlags::to_index(flag_t v)
{
  return (size_t)((int)v - (int)FIRST);
}

ParseFlags::flag_t ParseFlags::from_index(size_t i)
{
  return (flag_t)(i + (size_t)FIRST);
}

bool ParseFlags::is_set(flag_t flag) const
{
  RW_ASSERT(is_good(flag));
  index_t index = to_index(flag);
  return set_.test(index);
}

bool ParseFlags::is_clear(flag_t flag) const
{
  RW_ASSERT(is_good(flag));
  index_t index = to_index(flag);
  return !set_.test(index);
}

void ParseFlags::clear(flag_t flag)
{
  set(flag,false);
}

void ParseFlags::set(flag_t flag)
{
  set(flag,true);
}

void ParseFlags::set(flag_t flag, bool new_value)
{
  RW_ASSERT(is_good(flag));
  index_t index = to_index(flag);
  set_.set(index,new_value);
}

void ParseFlags::clear_inherit(flag_t flag)
{
  set_inherit(flag,false);
}

void ParseFlags::set_inherit(flag_t flag)
{
  set_inherit(flag,true);
}

void ParseFlags::set_inherit(flag_t flag, bool new_value)
{
  RW_ASSERT(is_good(flag));
  index_t index = to_index(flag);
  set_.set(index,new_value);
  inherit_.set(index,new_value);
}

void ParseFlags::clear_inherit_once(flag_t flag)
{
  set_inherit_once(flag,false);
}

void ParseFlags::set_inherit_once(flag_t flag)
{
  set_inherit_once(flag,true);
}

void ParseFlags::set_inherit_once(flag_t flag, bool new_value)
{
  RW_ASSERT(is_good(flag));
  index_t index = to_index(flag);
  set_.set(index,new_value);
  inherit_once_.set(index,new_value);
}

ParseFlags ParseFlags::copy_for_child() const
{
  ParseFlags new_flags;
  new_flags.inherit_ = inherit_;
  new_flags.set_ = inherit_once_ | inherit_;

  return new_flags;
}

void ParseFlags::clone_and_inherit(const ParseFlags& new_parent_flags)
{
  inherit_ |= new_parent_flags.inherit_;
  set_ |= new_parent_flags.inherit_once_ | new_parent_flags.inherit_;

  clear(ParseFlags::VISITED);
  clear(ParseFlags::C_FILLED);
  clear(ParseFlags::V_FILLED);
  clear(ParseFlags::V_LIST_KEYS);
  clear(ParseFlags::V_LIST_NONKEYS);
}


/*****************************************************************************/
AppDataParseTokenBase::AppDataParseTokenBase(
  unsigned i_app_data_index,
  const char* i_ns,
  const char* i_ext,
  AppDataCallback* i_callback,
  AppDataTokenBase i_ytoken)
: app_data_index(i_app_data_index),
  ns(i_ns),
  ext(i_ext),
  callback(i_callback),
  ytoken(i_ytoken)
{
}

bool AppDataParseTokenBase::parse_node_is_cached(
  const ParseNode* parse_node) const
{
  RW_ASSERT(parse_node);
  return parse_node->app_data_is_cached(*this);
}

bool AppDataParseTokenBase::parse_result_is_cached_first(
  const ParseLineResult* plr,
  ParseNode** parse_node) const
{
  RW_ASSERT(plr);
  return plr->app_data_is_cached_first(*this, parse_node);
}

bool AppDataParseTokenBase::parse_result_is_cached_last(
  const ParseLineResult* plr,
  ParseNode** parse_node) const
{
  RW_ASSERT(plr);
  return plr->app_data_is_cached_last(*this, parse_node);
}


/*****************************************************************************/
/**
 * Function that defines a parse node comparison.  Compares their CLI
 * keyword or value syntax, and that is equal, just compares their
 * pointers.
 */
bool ParseNodeSetSort::operator()(const ParseNode* a, const ParseNode* b)
{
  int cmp = strcmp(a->token_text_get(),b->token_text_get());
  if (cmp != 0) {
    return cmp < 0;
  }

  return a < b;
}


/*****************************************************************************/
ParseNode::ParseNode(BaseCli& cli)
: cli_(cli),
  parent_(NULL)
{}

ParseNode::ParseNode(BaseCli& cli, ParseNode* parent)
: cli_(cli),
  parent_(parent)
{
  if (parent_) {
    flags_ = parent_->flags_.copy_for_child();
    app_data_found_ = parent_->app_data_found_;
  }
}

ParseNode::ParseNode(const ParseNode& other, ParseNode* new_parent)
: cli_(other.cli_),
  parent_(new_parent),
  flags_(other.flags_),
  value_(other.value_)
{
  if (parent_) {
    flags_.clone_and_inherit(parent_->flags_);
    app_data_found_ = parent_->app_data_found_;
  }
}

ParseNode::~ParseNode()
{}

void ParseNode::adopt(ptr_t&& new_child)
{
  RW_ASSERT(&cli_ == &new_child->cli_);

  // ATTN: I don't think it is okay for adopted nodes to have a NULL
  // parent.  They should have me as a parent.
  RW_ASSERT(NULL == new_child->parent_ || new_child->parent_ == this);

  children_.emplace_back(std::move(new_child));
  // ATTN: need a flags member function to adjust the flags for adoption
}

ParseNode* ParseNode::find_clone(const ParseNode& other_root, const ParseNode& other_node)
{
  // I am the clone?
  if (is_clone(other_node)) {
    return this;
  }

  RW_ASSERT(is_clone(other_root)); // Must start at the same place!

  std::stack<const ParseNode*> nodes;
  const ParseNode* other_parent = &other_node;
  while(1) {
    nodes.push(other_parent);
    if (other_parent == &other_root) {
      break;
    }

    other_parent = other_parent->parent_;
    RW_ASSERT(other_parent); // Should have found the other root!
  }

  // Found the path to the other root  Now walk back down on our side.
  ParseNode* our_top = this;
  while(!nodes.empty()) {
    const ParseNode* other_top = nodes.top();
    nodes.pop();

    child_citer_t ci = our_top->children_.cbegin();
    for (; ci != our_top->children_.cend(); ++ci ) {
      if ((*ci)->is_clone(*other_top)) {
        our_top = ci->get();
        goto next_node;
      }
    }
    // Didn't find it.
    // ATTN: What kind of asserts should be here?
    return NULL;
  next_node:
    continue;
  }

  return our_top;
}

bool ParseNode::is_leafy() const
{
  return false;
}

bool ParseNode::is_key() const
{
  return false;
}

bool ParseNode::is_config() const
{
  return false;
}

bool ParseNode::is_rpc() const
{
  return false;
}

bool ParseNode::is_rpc_input() const
{
  return false;
}

bool ParseNode::is_notification() const
{
  return false;
}

bool ParseNode::is_value() const
{
  return false;
}

bool ParseNode::has_keys() const
{
  return false;
}

bool ParseNode::is_mode() const
{
  return false;
}

bool ParseNode::is_cli_print_hook() const
{
  return false;
}

const char* ParseNode::get_cli_print_hook_string()
{
  return nullptr;
}

bool ParseNode::is_built_in() const
{
  return false;
}

bool ParseNode::is_presence() const
{
  return false;
}

bool ParseNode::is_keyword() const
{
  return true;
}

bool ParseNode::is_sentence() const
{
  return false;
}

bool ParseNode::is_mandatory() const
{
  return false;
}

bool ParseNode::is_suppressed() const
{
  return false;
}

std::vector<std::string> ParseNode::check_for_mandatory() const
{
  // Let us assume by default that the mandatory check is passing
  // The check has to be perfomed in the Yang nodes
  return std::vector<std::string>();
}

bool ParseNode::ignore_mode_entry()
{
  return flags_.is_set(ParseFlags::IGNORE_MODE_ENTRY);
}

bool ParseNode::is_suitable (const ParseFlags& flags) const
{
  if (is_notification()) {
    return false;
  }

  if (is_suppressed()) {
    return false;
  }

  if (flags.is_set (ParseFlags::HIDE_VALUES)) {
    // Values are valid only if it is part of a key
    if (is_value() && !is_key()) {
      if (!flags.is_set (ParseFlags::CONFIG_ONLY) ||
          parent_->get_stmt_type() != RW_YANG_STMT_TYPE_LEAF_LIST) {
        return false;
      }
    }

    // Do not show RPC inside a show
    if (is_rpc()) {
      return false;
    }
  }

  if (cli_.state_ == BaseCli::STATE_CONFIG) {
    if (!is_config() && !(flags_.is_set(ParseFlags::GET_CONFIG))) {
      return false;
    }
  }

  // MODE_PATH is in effect only if no other "behavior node"
  // interfered, and the node is not an RPC

  if (flags.is_set (ParseFlags::MODE_PATH) &&
      !(is_rpc() || flags.is_set (ParseFlags::HIDE_VALUES))){
    if (!is_mode_path() && !is_key()) {
      return false;
    }
  }

  return true;
}

bool ParseNode::is_mode_path() const
{
  return false;
}

bool ParseNode::is_functional() const
{
  return false;
}

const char* ParseNode::get_ns()
{
  return NULL;
}

const char* ParseNode::get_prefix()
{
  return NULL;
}

rw_yang_stmt_type_t ParseNode::get_stmt_type() const
{
  return RW_YANG_STMT_TYPE_NA;
}

void ParseNode::set_mode(const char* display)
{
  UNUSED(display);
  RW_CRASH();
}

void ParseNode::set_cli_print_hook(const char* api)
{
  UNUSED(api);
  RW_CRASH();
}

void ParseNode::set_suppressed()
{
}

ParseNode* ParseNode::mode_enter_top()
{
  return NULL;
}

const char* ParseNode::get_prompt_string()
{
  return token_text_get();
}

const std::string& ParseNode::value_get() const
{
  return value_;
}

void ParseNode::value_set(const std::string& value)
{
  // ATTN: In debug build, validate that the new value actually matches?
  flags_.set(ParseFlags::HAS_VALUE);
  value_ = value;
}

bool ParseNode::value_is_match(
                const std::string& value,
                bool check_prefix) const
{
  (void)check_prefix;
  const char* token_text = token_text_get();
  const char* value_text = value.c_str();
  size_t i;
  for (i = 0;
       token_text[i] != '\0' && value_text[i] != '\0' && token_text[i] == value_text[i];
       ++i) {
    ; // null loop
  }

  if (value_text[i] == '\0' ) {
    return true;
  }
  return false;
}

const std::string& ParseNode::value_complete(const std::string& value)
{
  // ATTN: In debug build, validate that the new value actually matches?
  (void)value;
  value_set(token_text_get());
  return value_;
}

void ParseNode::hide(ParseNode* child)
{
  RW_ASSERT(child);
  if (is_hide_key_keyword() && child->is_key()) {
    // In listy mode with keys, key keywords are supressed
    return;
  }
  visible_t::size_type count = visible_.erase(child);
  visible_t::iterator it = visible_.begin();

  // erase other elements that are in conflict with the selected child due
  // to choices and cases
  

  while (visible_.end() != it) {
    if (child->is_conflicting_node (*it)) {
        visible_.erase(it++);
      }
      else {
        it++;
      }
  }
}

void ParseNode::next_hide(ParseNode* child, ParseLineResult* plr)
{
  hide(child);
  next(plr);
}

void ParseNode::next_hide_all(ParseLineResult* plr)
{
  visible_.clear();
  next(plr);
}

void ParseNode::clear_children()
{
  children_.clear();
  visible_.clear();
  flags_.clear(ParseFlags::C_FILLED);
  flags_.clear(ParseFlags::V_FILLED);
  flags_.clear(ParseFlags::V_LIST_KEYS);
  flags_.clear(ParseFlags::V_LIST_NONKEYS);
}

void ParseNode::fill_children()
{
}

void ParseNode::fill_visible_all()
{
  if (flags_.is_clear(ParseFlags::C_FILLED)) {
    fill_children();
  } else {
    visible_.clear();
  }

  for (child_iter_t ci = children_.begin(); ci != children_.end(); ++ci) {
    if ((*ci)->is_suitable (flags_)) {
      visible_.insert(ci->get());
    }
  }

  add_builtin_behavorial_to_visible();
  flags_.set(ParseFlags::V_FILLED);

}

void ParseNode::fill_rpc_input()
{
}

void ParseNode::add_builtin_behavorial_to_visible()
{
}

void ParseNode::add_visible(ParseNode* visible)
{
  visible_.insert(visible);
}


void ParseNode::get_completions(
  completions_t* completions,
  const std::string& value)
{
  // Create a token_set with the list of visible items. If there is conflicting
  // token, then the token_set.count(token) will return more than 1.
  std::multiset<std::string> token_set;
  for (ParseNode* child : visible_) {
    token_set.insert(child->token_text_get());
  }

  // Only the visible children are possible completions
  for (vis_iter_t vi = visible_.begin(); vi != visible_.end(); ++vi) {
    ParseNode* child = *vi;

    bool prefix_required = (token_set.count(child->token_text_get()) > 1);

    if (child->is_keyword()) {
      if(child->value_is_match(value, prefix_required)) {
        completions->emplace_back(child, prefix_required);
      }
    } else {
      // Value node...
      if (value.length() == 0) {
        // Until the user enters something, all values are matches.
        completions->emplace_back(child);
      } else if(child->value_is_match(value, prefix_required)) {
        // Value node...
        // ATTN: Continue to assume that all values are matches?
        completions->emplace_back(child);
        // ATTN: Stop looking for matches, or only continue to look for keyword matches?
      }
    }
  }
}

std::string ParseNode::descendants_path() const
{
  std::string str = "/" + value_get();
  if (children_.size()) {
    RW_ASSERT(children_.size() == 1);
    str.append(children_.cbegin()->get()->descendants_path());
  }
  return str;
}

std::string ParseNode::ancestor_path(ParseNode* stop) const
{
  std::string str("/");
  if (this == stop) {
    return str;
  }
  str.append(value_get());
  if (NULL == parent_) {
    RW_ASSERT(NULL == stop);
    return str;
  }
  return parent_->ancestor_path(stop) + str;
}

ParseNode* ParseNode::path_search(std::string path)
{
  RW_ASSERT(path.size());
  RW_ASSERT(path[0]=='/');
  path.erase(0, 1);
  std::size_t pos = path.find('/');

  std::string subnode_val;
  std::string pending_path;
  if (pos != std::string::npos) {
    subnode_val = path.substr(0, pos);
    pending_path = path.substr(pos);
  } else {
    // this is the last node in the path
    subnode_val = path;
    pending_path.clear();
  }

  for (child_iter_t ci = children_.begin(); ci != children_.end(); ++ci) {
    ParseNode* child = ci->get();
    if (subnode_val == child->value_get()) {
      // the current node matches the subnode value
      if (pending_path.length()) {
        // more nodes to search, so call path_search recurisvely
        return child->path_search(pending_path);
      } else {
        // Ok found a node!
        return child;
      }
    }
  }
  return NULL;
}

XMLNode* ParseNode::create_xml_node(XMLDocument *doc, XMLNode *parent)
{
  return parent->add_child(value_.c_str(), nullptr, get_ns(), 
                        get_prefix());
}

XMLNode* ParseNode::get_xml_node(XMLDocument *doc, XMLNode *parent)
{
  XMLNode* node = parent->find(value_.c_str());
  if (!node) {
    node = create_xml_node(doc, parent);
  }

  RW_ASSERT(node);
  return node;
}

CliCallback* ParseNode::get_callback()
{
  return nullptr;
}

void ParseNode::add_descendent(ParseNodeFunctional *desc)
{
  UNUSED (desc);
}

ParseNodeFunctional* ParseNode::find_descendent(const std::string& token)
{
  UNUSED(token);
  return nullptr;
}

ParseNodeFunctional* ParseNode::find_descendent_deep(
                        const std::vector<std::string>& path)
{
  UNUSED(path);
  return nullptr;
}

XMLNode* ParseNode::update_xml_for_leaf_list(XMLDocument *doc,
                                         XMLNode *parent,
                                         const char *attr)
{
  XMLNode *current = parent->find_value(value_.c_str(), attr, get_ns());

  if (NULL == current) {
    current = create_xml_node(doc, parent);
    current->set_text_value(attr);
  }
  return current;
}

bool ParseNode::app_data_is_cached(const AppDataParseTokenBase& adpt) const
{
  // default is no support for cached application data
  UNUSED(adpt);
  return false;
}

bool ParseNode::app_data_check_and_get(const AppDataParseTokenBase& adpt, intptr_t* data) const
{
  // default is no support for cached application data
  UNUSED(adpt);
  UNUSED(data);
  return false;
}

void ParseNode::app_data_clear()
{
  app_data_found_.clear();
}

void ParseNode::print_tree(unsigned indent, std::ostream& os) const
{
  std::string name = token_text_get();
  os << std::string(indent, ' ') << (void*)this << "="
     << name << ": '" << value_get() << "'" << std::endl;
  indent = indent+4;
  for (child_citer_t ci = children_.cbegin(); ci != children_.cend(); ++ci) {
    ci->get()->print_tree(indent,os);
  }
}

bool ParseNode::is_conflicting_node (ParseNode *other)
{
  return false;
}

bool ParseNode::is_hide_key_keyword ()
{
  return false;
}
  

/*****************************************************************************/
/**
 * Create a parse node that is associated with a model node and has a
 * parent parse node.  This is the typical use case.
 */
ParseNodeYang::ParseNodeYang(BaseCli& cli, YangNode* yangnode, ParseNode* parent)
  : ParseNode(cli,parent),
    yangnode_(yangnode),
    last_key_child_(children_.end())
{
  RW_ASSERT(yangnode_); // If you want to define a builtin, please create a new subclass
}

/**
 * Copy construct a new parse node from another node, setting the parent.
 */
ParseNodeYang::ParseNodeYang(const ParseNodeYang& other, ParseNode* new_parent)
  : ParseNode(other, new_parent),
    yangnode_(other.yangnode_),
    last_key_child_(children_.end())
{
  RW_ASSERT(yangnode_); // If you want to define a builtin, please create a new subclass

}

/**
 * Clone a yang schema node.  This function provides a strong exception
 * guarantee - if this function throws, the new node (tree) is deleted,
 * new_parent is left in a consistent state, and nothing leaks.
 */
ParseNode::ptr_t ParseNodeYang::clone(ParseNode* new_parent) const
{
  RW_ASSERT(NULL == new_parent || &cli_ == &new_parent->cli_);
  ptr_t new_clone(new ParseNodeYang(*this,new_parent));
  return new_clone;
}

/**
 * Check if another node is a clone of me.  Uses the yangnode_ pointer
 * for the comparison.
 */
bool ParseNodeYang::is_clone(const ParseNode& other) const
{
  if (this == &other) {
    // Cannot be a clone of self.
    return false;
  }
  clone_visit_t cv(this);
  return other.is_clone_visit(&cv, sizeof(cv));
}

/**
 * Check if another node is a clone of me.  Uses the yangnode_ pointer
 * for the comparison.
 */
bool ParseNodeYang::is_clone_visit(const void* ocv, size_t size) const
{
  if (size != sizeof(clone_visit_t)) {
    return false;
  }
  clone_visit_t cv(this);
  return 0 == memcmp(&cv,ocv,size);
}

/**
 * Is the parse node leafy?
 */
bool ParseNodeYang::is_leafy() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_leafy();
}

/**
 * Is the parse node a list key node?
 */
bool ParseNodeYang::is_key() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_key();
}

/**
 * Is the parse node a configuration node?
 */
bool ParseNodeYang::is_config() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_config();
}

/**
 * Is the parse node a RPC definition?
 */
bool ParseNodeYang::is_rpc() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_rpc();
}

/**
 * Is the parse node a descendent of a RPC node?
 */
bool ParseNodeYang::is_rpc_input() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_rpc_input();
}

/**
 * Is the parse node a descendent of a notification node?
 */
bool ParseNodeYang::is_notification() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_notification();
}

/**
 * No, a schema node is not a value.
 */
bool ParseNodeYang::is_value() const
{
  return false;
}

/**
 * Is the parse node a list node with keys?
 */
bool ParseNodeYang::has_keys() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->has_keys();
}

/**
 * Is the parse node a mode node?
 */
bool ParseNodeYang::is_mode() const
{
  RW_ASSERT(yangnode_);

  rw_yang_stmt_type_t const stmt_type = yangnode_->get_stmt_type();
  switch (stmt_type) {
    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_GROUPING:
      if (   is_config()
          && flags_.is_set(ParseFlags::AUTO_MODE_CFG_CONT)) {
        return true;
      }
      break;

    case RW_YANG_STMT_TYPE_LIST:
      if (   is_config()
          && has_keys()
          && flags_.is_set(ParseFlags::AUTO_MODE_CFG_LISTK)) {
        return true;
      }
      break;

    default:
      break;
  }

  if (flags_.is_set (ParseFlags::HIDE_MODES)) {
    // do not indicate modes in NO and SHOW behavior node interactions
    return false;
  }
  return cli_.ms_adpt_.parse_node_is_cached(this);
}

/**
 * Is the parse node a cli print hook node?
 */
bool ParseNodeYang::is_cli_print_hook() const
{
  RW_ASSERT(yangnode_);
  return cli_.ph_adpt_.parse_node_is_cached(this);
}

/**
 * Get the CLI print hook string.  Buffer owned by the library.
 * Caller may use string until model is destroyed.
 */
const char* ParseNodeYang::get_cli_print_hook_string()
{
  RW_ASSERT(yangnode_);
  const char* print_hook = nullptr;
  // Must call the function for its side-effects!
  if (is_cli_print_hook()) {
    yangnode_->app_data_check_lookup_and_get(cli_.ph_adpt_.get_yang_token(), &print_hook);
  }
  return print_hook;
}


bool ParseNodeYang::is_presence() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_presence();
}

/**
 * Schema nodes are always keywords.
 */
bool ParseNodeYang::is_keyword() const
{
  return true;
}

/**
 * Determine if a command is complete, from this node down.  Command is
 * complete if all visited nodes are also complete.
 */
bool ParseNodeYang::is_sentence() const
{
  // Cannot be complete if never visted this node.
  if (! flags_.is_set(ParseFlags::VISITED)) {
    return false;
  }

  // ATTN: mode-list enter: need to specify all keys before
  //   command is complete
  // ATTN: mode-enter needs its own recursive check function
  //   that verifies that just the keys have been entered.

  if (children_.size() == 0) {
    // ATTN: RIFT-994 in JIRA = This should happen only for a leaf of type empty
    return true;
  }
  bool visited = false;

  for (child_citer_t vi = children_.cbegin(); vi != children_.cend(); ++vi) {
    if ((*vi)->flags_.is_set(ParseFlags::VISITED)) {
      if (! (*vi)->is_sentence()) {
        return false;
      }
      visited = true;
    }
  }

  // Visited at least one child, it was good.  So this command is good.
  if (visited) {
    return true;
  }

  // Even without visiting any children, this command may be good if it enters a mode.
  if ((is_mode() && ! has_keys()) ||
      (is_presence())){
    return true;
  }

  // Visited, but not mode.  Incomplete.
  return false;
}

bool ParseNodeYang::is_mandatory() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_mandatory();
}

bool ParseNodeYang::is_suppressed() const
{
  return cli_.suppress_adpt_.parse_node_is_cached(this);
}

std::vector<std::string> ParseNodeYang::check_for_mandatory() const 
{
  std::vector<std::string> missing;
  for (child_citer_t vi = children_.cbegin(); vi != children_.cend(); ++vi) {
    if ((*vi)->is_mandatory() && !(*vi)->flags_.is_set(ParseFlags::VISITED)) {
      // Mandatory node not visited! check fails
      missing.push_back((*vi)->token_text_get());
    }
  }

  if (!missing.empty()) {
    return missing;
  }

  // If the mandatory check passes at the current level, go down the next level
  // of parsed children
  for (child_citer_t vi = children_.cbegin(); vi != children_.cend(); ++vi) {
    // For visited child go down the child tree to see if the check passes
    if ((*vi)->flags_.is_set(ParseFlags::VISITED))
    {
      missing = (*vi)->check_for_mandatory();
      if (!missing.empty()) {
        // Report missing for one child at a time
        return missing;
      }
    }
  }

  return missing;
}

/**
 * get the statement type of the underlying yang node
 */
rw_yang_stmt_type_t ParseNodeYang::get_stmt_type() const {
  RW_ASSERT(yangnode_);
  return yangnode_->get_stmt_type();
}

/**
 * set the mode in the underlying yang node
 */
void ParseNodeYang::set_mode(const char *display) {
  RW_ASSERT (yangnode_);
  yangnode_->app_data_set_and_give_ownership(cli_.ms_adpt_.get_yang_token(), (const char*)RW_STRDUP(display));
  yangnode_->set_mode_path_to_root();
}

/**
 * set the plugin api in the underlying yang node
 */
void ParseNodeYang::set_cli_print_hook(const char *api) {
  RW_ASSERT (yangnode_);
  yangnode_->app_data_set_and_give_ownership(cli_.ph_adpt_.get_yang_token(), (const char*)RW_STRDUP(api));
}

void ParseNodeYang::set_suppressed() {
  RW_ASSERT (yangnode_);
  yangnode_->app_data_set_and_keep_ownership(
              cli_.suppress_adpt_.get_yang_token(), "");
}

/**
 * Determine if a command is a mode entering command, and if so, return
 * the ParseNode that defines the mode.  A mode entering command ends
 * at a container if it is a mode, or ends at a list (with specified
 * keys) if it is a mode.  Further descent into the mode node precludes
 * mode entering!
 */
ParseNode* ParseNodeYang::mode_enter_top()
{
  // Cannot be a mode-enter if never visted this node.
  if (! flags_.is_set(ParseFlags::VISITED)) {
    return NULL;
  }

  rw_yang_stmt_type_t const stmt_type = yangnode_->get_stmt_type();

  ParseNode* child = NULL;
  unsigned visited = 0;
  unsigned visited_keys = 0;
  unsigned missed_keys = 0;
  for (child_citer_t ci = children_.cbegin(); ci != children_.cend(); ++ci) {
    if ((*ci)->flags_.is_set(ParseFlags::VISITED)) {
      ++visited;
      child = (*ci).get();

      if ((*ci)->is_key()) {
        ++visited_keys;
      }
    }
    else if ((*ci)->is_key()) {
      ++missed_keys;
    }
  }

  if (0 == visited) {
    // This object is a mode, and didn't descend.
    // This is good unless it is a list with keys.
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_ROOT:
      case RW_YANG_STMT_TYPE_CONTAINER:
      case RW_YANG_STMT_TYPE_GROUPING:
      case RW_YANG_STMT_TYPE_RPCIO:
      case RW_YANG_STMT_TYPE_RPC: // ATTN: Really?

        if (is_mode()) {
          return this;
        }
        return NULL;

      case RW_YANG_STMT_TYPE_LIST:
        if (is_mode() && 0 == missed_keys) {
          return this;
        }
        return NULL;

      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
      case RW_YANG_STMT_TYPE_NOTIF:
      case RW_YANG_STMT_TYPE_NA:
      case RW_YANG_STMT_TYPE_ANYXML:
        RW_ASSERT(!is_mode());
        return NULL;

      default:
        break;
    }
    RW_ASSERT_NOT_REACHED();
  }

  // Visited at least one child...

  if (RW_YANG_STMT_TYPE_LIST == stmt_type) {
    // Check that all the keys were visited, and no non-keys were visited.
    if (   is_mode()
        && 0 == missed_keys
        && visited == visited_keys) {
      return this;
    }
    return NULL;
  }

  if (1 == visited) {
    // Recursively try the one visited child.
    return child->mode_enter_top();
  }

  // Visited more than one child, can't be a mode enter.
  return NULL;
}

/**
 * Get a string that represents the keyword, or value type, that can be
 * used for identifying and sorting keywords in the CLI for help.
 */
const char* ParseNodeYang::token_text_get() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->get_name();
}

const char* ParseNodeYang::get_prompt_string()
{
  RW_ASSERT (yangnode_);
  const char* mode_string = NULL;

  std::string token = std::string(token_text_get());
  std::string::iterator last_space = remove(token.begin(), token.end(), ' ');
  if (last_space != token.end()) {
    token.erase(last_space, token.end());
  }

  if ((cli_.ms_adpt_.parse_node_is_cached(this))) {
    // remove spaces from both strings
    yangnode_->app_data_check_lookup_and_get(cli_.ms_adpt_.get_yang_token(), &mode_string);
    std::string mod = std::string(mode_string);
    last_space = remove(mod.begin(), mod.end(), ' ');
    if (last_space != mod.end()) {
      mod.erase(last_space, mod.end());
    }

    if (mod != token) {
      yangnode_->app_data_check_lookup_and_get(cli_.ms_adpt_.get_yang_token(), &mode_string);
      return mode_string;
    }
  }

  prompt_ = token;

  if (!has_keys() && !is_key()) {
    return token_text_get();
  }

  if (has_keys()) {
    for (child_iter_t ci = children_.begin(); ci != children_.end(); ++ci) {
      if ((*ci)->is_key()) {
          prompt_.append("-");
          prompt_.append((*ci)->get_prompt_string());
        }
    }

    return prompt_.c_str();
  }

  RW_ASSERT (is_key());
  RW_ASSERT (children_.begin() != children_.end());
  // The prompt already has this nodes name. Add the child (which has to be a
  // value node) value to this.
  prompt_.append("-");
  prompt_.append((*children_.begin())->value_get());

  return prompt_.c_str();
}
/**
 * Get a short help string.  The short help string is used in the CLI
 * '?' completion list, to summarize each command or value.  It should
 * not contain embedded newlines.
 */
const char* ParseNodeYang::help_short_get() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->get_help_short();
}

/**
 * Get a full help string.  The full help string is used by the CLI
 * help command, or when displaying help with a shell command line
 * argument.  It may contain embedded newlines.
 */
const char* ParseNodeYang::help_full_get() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->get_help_full();
}

/**
 * Attempt to match a string value to the node.  If a keyword, a prefix
 * matches.  If a parsed value, then the underlying yang model must
 * parse the value to determine if it is good.
 */
bool ParseNodeYang::value_is_match(
        const std::string& value, 
        bool check_prefix) const
{
  // The value may contain yang module prefix - ns_prefix:keyword
  // Split the prefix and find a match
  std::string keyword;

  size_t pos = value.find(':');
  if (pos != std::string::npos) {
    std::string ns_prefix = value.substr(0, pos);
    const char* ynode_prefix = yangnode_->get_prefix();
    if (strncmp(ynode_prefix, ns_prefix.c_str(), ns_prefix.size()) != 0) {
      // Prefix starts_with match failed
      return false;
    }
    keyword = value.substr(pos+1, std::string::npos);
  } else {
    if (check_prefix) {
      // The value doesn't have ':', but check_prefix is set, which means that
      // the token has a conflict and has to match the prefix instead of the
      // keyword.
      const char* ynode_prefix = yangnode_->get_prefix();
      if (strncmp(ynode_prefix, value.c_str(), value.size()) == 0) {
        return true;
      }
      return false;
    }
    keyword = value;
  }
  return yangnode_->matches_prefix(keyword.c_str());
}

/**
 * Determine the root node for parsing the next word in the line
 * buffer.  This function may have been called directly from the line
 * parser, of from a child node that decided that it is has been fully
 * consumed.
 */
void ParseNodeYang::next(ParseLineResult* plr)
{
  flags_.set(ParseFlags::VISITED);

  if (is_rpc_input() && get_stmt_type() != RW_YANG_STMT_TYPE_LIST) {
    // For RPC inputs by default hide the node that is visited.
    // Lists are exceptions as they can be revisted again and again
    parent_->hide(this);
  }

  /*
   * Cases:
   *  - anyxml:
   *     - ATTN: Not implemented yet.
   *  - leaf:
   *     - never visited before:
   *        - type empty: populate children, return parent to try siblings
   *        - all-others: populate values, return self
   *     - visited before:
   *        - consume self
   *        - ATTN: I suspect there are corner cases here...
   *  - leaf-list:
   *     - never visited before:
   *        - populate value(s), return self
   *     - visited before:
   *        - ATTN: Need to find a way to prevent this from happening?
   *          Once you leave a leaf-list, you shouldn't come back into it
   *  - list:
   *     - never visited before:
   *        - populate keys(s), return self
   *     - in keys mode:
   *        - more keys:
   *           - return self
   *        - no more keys:
   *           - populate others, return self
   *     - in others mode:
   *        - more data:
   *           - return self
   *        - no more data
   *           - consume self
   *  - rpc, container
   *     - never visited before:
   *        - all-others: populate values, return self
   *     - visited before:
   *        - more data:
   *           - return self
   *        - no more data
   *           - consume self
   */
  rw_yang_stmt_type_t const stmt_type = yangnode_->get_stmt_type();
  if (!flags_.is_set(ParseFlags::V_FILLED)) {
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_ROOT:
      case RW_YANG_STMT_TYPE_CONTAINER:
      case RW_YANG_STMT_TYPE_GROUPING:
      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
      case RW_YANG_STMT_TYPE_RPC:
        // Populate all children
        fill_visible_all();

        // For an empty leaf node or a presence container return the siblings.
        // For hidden nodes return self
        if (visible_.empty() && 
            (stmt_type == RW_YANG_STMT_TYPE_LEAF || 
             (stmt_type == RW_YANG_STMT_TYPE_CONTAINER &&
              yangnode_->is_presence())
            ) &&
            !flags_.is_set(ParseFlags::HIDE_VALUES))
        {
          plr->result_node_ = plr->result_node_->parent_;
          plr->parse_node_ = parent_;
          parent_->next_hide(this, plr);
        }
        return;

      case RW_YANG_STMT_TYPE_RPCIO:
        RW_CRASH();
        break;

      case RW_YANG_STMT_TYPE_LIST:
        // Populate the keys
        fill_visible_keys();
        if (visible_.empty()) {
          // No keys - break to populate the others
          break;
        }
        return;

      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
      case RW_YANG_STMT_TYPE_NOTIF:
      case RW_YANG_STMT_TYPE_NA:
      case RW_YANG_STMT_TYPE_ANYXML:
      default:
        RW_ASSERT_NOT_REACHED();
    }
  }

  // ATTN: There should be parsing mode that prevent popping out of things!

  /*
   * Been here before.  One of our children thought that parsing should
   * continue here.  We may want to punt further up, or continue here.
   */
  switch (stmt_type) {
    case RW_YANG_STMT_TYPE_ROOT:
    case RW_YANG_STMT_TYPE_RPC:
      // Can't pop these.
      return;

    case RW_YANG_STMT_TYPE_LEAF:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      // ATTN: These could get tricky with type bits...
      break;

    case RW_YANG_STMT_TYPE_LIST:
      if (last_key_child_ != children_.end()) {
        // More keys possible and the node is a mode where key-keywords 
        // are suppressed
        fill_visible_keys();
      }
      if (visible_.empty() && !flags_.is_set(ParseFlags::V_LIST_NONKEYS)) {
        fill_visible_nonkeys();

        if (is_rpc_input()) {
          // For RPC input node, also make the list node visible in parent
          // Lists can be specified multiple times, but only after all keys are
          // filled.
          parent_->add_visible(this);
        }
      }
      // Popping otherwise depends on parent and children...
      break;

    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_GROUPING:
      // Popping depends on parent and children...
      break;

    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_NOTIF:
    case RW_YANG_STMT_TYPE_NA:
    case RW_YANG_STMT_TYPE_ANYXML:
    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (flags_.is_set(ParseFlags::PARSE_TOP)) {
    // Can't pop if this node is the current mode node.  Stay here.
    return;
  }

  RW_ASSERT(parent_);
  RW_ASSERT(plr->result_node_->parent_);

  if (visible_.empty()) {
    // for keys, always display siblings. There is no way to do this
    // for non-key values for lists.
    // Also needed for show commands
    if (is_key() || (flags_.is_set(ParseFlags::NEXT_TO_SIBLING))) {
      // Pop self to try siblings - no more keywords here.
      plr->result_node_ = plr->result_node_->parent_;
      plr->parse_node_ = parent_;

      if (is_rpc_input() && get_stmt_type() == RW_YANG_STMT_TYPE_LIST) {
        // For RPC input lists as they can be specified multiple times, 
        // don't hide it
        parent_->next(plr);
      } else {
        parent_->next_hide(this, plr);
      }
      return;
    }

    // ATTN: Need special case for list+keys+mode: when a list is a
    //       mode, user must enter all keys in order to enter the mode!

    // Do not pop to my siblings; stay here.
    return;
  }

  // Stay here, I guess.
  return;
}

/**
 * Rebuild the list of possible children nodes.  This will also reset
 * the list of visible children to be all the children.
 */
void ParseNodeYang::fill_children()
{
  if (flags_.is_set(ParseFlags::C_FILLED)) {
    return;
  }

  flags_.clear(ParseFlags::V_FILLED);
  flags_.clear(ParseFlags::V_LIST_KEYS);
  flags_.clear(ParseFlags::V_LIST_NONKEYS);
  children_.clear();
  visible_.clear();

  if (yangnode_->is_leafy()) {
    // Allocate a ParseNodeValue for each value
    for (YangValueIter yvi = yangnode_->value_begin(); yvi != yangnode_->value_end(); ++yvi) {
      ptr_t new_node(new ParseNodeValue(cli_,yangnode_,&*yvi,this));
      children_.push_back(std::move(new_node));
    }
  } else if (!yangnode_->is_rpc()){
    // Allocate a ParseNodeYang for each child schema node
    for (YangNodeIter yni = yangnode_->child_begin(); yni != yangnode_->child_end(); ++yni) {
      ptr_t new_node(new ParseNodeYang(cli_,&*yni,this));
      if (new_node->is_rpc()) {

        // RPC input nodes - no hide input or mode path restrictions should apply once
        // inside an RPC
        new_node->flags_.clear_inherit (ParseFlags::MODE_PATH);
        new_node->flags_.clear_inherit (ParseFlags::HIDE_VALUES);
      }

      children_.push_back(std::move(new_node));

    }
  } else {
    fill_rpc_input();
  }

  last_key_child_ = children_.end();
  flags_.set(ParseFlags::C_FILLED);
}

void ParseNodeYang::fill_rpc_input()
{
  RW_ASSERT (RW_YANG_STMT_TYPE_RPC == yangnode_->get_stmt_type());
  YangNodeIter input = yangnode_->child_end();

  if (flags_.is_set(ParseFlags::C_FILLED)) {
    return;
  }

  for (YangNodeIter yni = yangnode_->child_begin(); yni != yangnode_->child_end(); ++yni) {
    if (yni->is_rpc_input()) {
      input = yni;
    }
  }

  flags_.clear(ParseFlags::V_FILLED);
  flags_.clear(ParseFlags::V_LIST_KEYS);
  flags_.clear(ParseFlags::V_LIST_NONKEYS);
  children_.clear();
  visible_.clear();

  if (yangnode_->child_end() != input) {
    for (YangNodeIter yni = input->child_begin(); yni != input->child_end(); ++yni) {
      ptr_t new_node(new ParseNodeYang(cli_,&*yni,this));
      children_.push_back(std::move(new_node));
    }
  }

  flags_.set(ParseFlags::C_FILLED);
}

/**
  * Get the prefix of the underlying YANG node
 */
const char* ParseNodeYang::get_prefix() {
  RW_ASSERT (yangnode_);
  return (yangnode_->get_prefix());
}

/**
 * Get the namespace of the underlying YANG node
 */
const char* ParseNodeYang::get_ns() {
  RW_ASSERT (yangnode_);
  return (yangnode_->get_ns());
}


void ParseNodeYang::add_builtin_behavorial_to_visible()
{
  // Fill the list from the yang definitions first, since the visible list
  // could be reset there
  ParseNodeBehavior *tmp = nullptr;

  if (!flags_.is_set (ParseFlags::PARSE_TOP)) {
    return;
  }

  if ((cli_.mode_stack_.size() == 1) &&
      (BaseCli::STATE_CONFIG != cli_.state_ )){
    // For root node, add the functionals
    for (ParseNodeFunctional::desc_citer_t citer = cli_.top_funcs_.cbegin();
         citer != cli_.top_funcs_.cend(); citer++) {

      ParseNodeFunctional::d_ptr_t temp (new ParseNodeFunctional (*citer->get(), 0));
      visible_.insert (temp.get());
      children_.push_back (std::move (temp));
    }
  }

  // Add end and exit for modes
  // ATTN: And not root mode

  if (cli_.builtin_exit_.get() &&
      ((cli_.mode_stack_.size() > 1) ||
       (BaseCli::STATE_CONFIG == cli_.state_ )))
  {
    visible_.insert(cli_.builtin_exit_.get());
    visible_.insert(cli_.builtin_end_.get());
  }

  // if configuration is not set, config is a valid child
  if ((BaseCli::STATE_CONFIG != cli_.state_) && (nullptr != cli_.config_node_.get())) {
    tmp = dynamic_cast <ParseNodeBehavior *> (cli_.config_node_.get());
    ptr_t config_node(new ParseNodeBehavior (cli_, *tmp, this));
    visible_.insert(config_node.get());
    children_.push_back (std::move (config_node));
  }

  else {
    if (nullptr != cli_.no_node_.get()){
      tmp = dynamic_cast <ParseNodeBehavior *> (cli_.no_node_.get());
      ptr_t no_node(new ParseNodeBehavior (cli_, *tmp, this));
      visible_.insert(no_node.get());
      children_.push_back (std::move(no_node));
    }
    if (nullptr != cli_.commit_node_.get()) {
      // Know for sure that this is a ParseNodeFunctional. 
      // Hence doing a static cast.
      ParseNodeFunctional* fn = static_cast<ParseNodeFunctional *>(cli_.commit_node_.get());
      ptr_t commit_node(new ParseNodeFunctional (cli_, *fn, this));
      visible_.insert(commit_node.get());
      children_.push_back(std::move(commit_node));
    }
    if (nullptr != cli_.discard_node_.get()) {
      // Know for sure that this is a ParseNodeFunctional. 
      // Hence doing a static cast.
      ParseNodeFunctional* fn = static_cast<ParseNodeFunctional *>(cli_.discard_node_.get());
      ptr_t discard_node(new ParseNodeFunctional (cli_, *fn, this));
      visible_.insert(discard_node.get());
      children_.push_back(std::move(discard_node));
    }
  }

  // show is always valid?
  if (nullptr != cli_.show_node_.get()) {
    tmp = dynamic_cast <ParseNodeBehavior *> (cli_.show_node_.get());

    ptr_t show_node(new ParseNodeBehavior (cli_, *tmp, this));
    visible_.insert(show_node.get());
    children_.push_back (std::move(show_node));
  }
}

void ParseNodeYang::fill_visible_keys()
{
  if (flags_.is_clear(ParseFlags::C_FILLED)) {
    fill_children();
  } else {
    visible_.clear();
  }

  if (!has_keys()) {
    // This also sets up the is_key_ field in any children - which is required
    // for the rest of cli to work correctly
    return;
  }

  if (flags_.is_set (ParseFlags::V_LIST_KEYS_CLONED)) {
    fill_visible_nonkeys();

    // for top level and modes, add behavioral and builtins
    add_builtin_behavorial_to_visible();
    return;
  }

  if (is_hide_key_keyword()) {
    // Show only one key value at a time, in the yang order
    // Remember the last visible key child 
    child_iter_t ci;
    if (last_key_child_ == children_.end()) {
      ci = children_.begin();
    } else {
      ci = ++last_key_child_;
    }

    for (; ci != children_.end(); ++ci) {
      ParseNode* pn = ci->get();

      if (pn->is_key() && pn->is_suitable(flags_)) {
        // Mark the use of key keyword as deprecated
        // Deprecated nodes will not shown in completions
        pn->flags_.set(ParseFlags::DEPRECATED);
        visible_.insert(pn);

        if (!pn->flags_.is_set(ParseFlags::C_FILLED)) {
          pn->fill_children();
          pn->fill_visible_all();
        }
        for (auto ch_visible : pn->visible_) {
          visible_.insert(ch_visible);
        }
        break;
      }
    }
    last_key_child_ = ci;
   
    if (flags_.is_set(ParseFlags::GET_GENERIC) 
        && last_key_child_ != children_.end()) {
      // For show commands include a wildcard in place of keys
      visible_.insert(new ParseNodeValueWildcard(cli_, ci->get()));
    }
  } else {
    for (child_iter_t ci = children_.begin(); ci != children_.end(); ++ci) {
      if ((*ci)->is_key() && (*ci)->is_suitable(flags_)) {
        visible_.insert(ci->get());
      }
    }
  }


  flags_.set(ParseFlags::V_FILLED);
  flags_.set(ParseFlags::V_LIST_KEYS);
}

void ParseNodeYang::fill_visible_nonkeys()
{
  if (flags_.is_clear(ParseFlags::C_FILLED)) {
    fill_children();
  } else {
    visible_.clear();
  }

  for (child_iter_t ci = children_.begin(); ci != children_.end(); ++ci) {
    if (!(*ci)->is_key() && (*ci)->is_suitable(flags_)) {
      visible_.insert(ci->get());
    }
  }
  flags_.set(ParseFlags::V_FILLED);
  flags_.set(ParseFlags::V_LIST_KEYS);
  flags_.set(ParseFlags::V_LIST_NONKEYS);
}

bool ParseNodeYang::is_mode_path() const
{
  RW_ASSERT (yangnode_);
  return yangnode_->is_mode_path();
}

bool ParseNodeYang::app_data_is_cached(const AppDataParseTokenBase& adpt) const
{
  RW_ASSERT(adpt.app_data_index < cli_.app_data_tokens_.size());
  RW_ASSERT(yangnode_);
  bool is_cached = yangnode_->app_data_is_cached(&adpt.ytoken);
  if (is_cached) {
    return true;
  }
  if (!yangnode_->app_data_is_looked_up(&adpt.ytoken)) {
    // Need to do a junk lookup, in order to ensure that the proper
    // cache state is known.
    intptr_t junk_data;
    return yangnode_->app_data_check_lookup_and_get(&adpt.ytoken, &junk_data);
  }

  return false;
}

bool ParseNodeYang::app_data_check_and_get(const AppDataParseTokenBase& adpt, intptr_t* data) const
{
  RW_ASSERT(adpt.app_data_index < cli_.app_data_tokens_.size());
  RW_ASSERT(yangnode_);
  return yangnode_->app_data_check_lookup_and_get(&adpt.ytoken, data);
}

bool ParseNodeYang::is_conflicting_node (ParseNode *ot)
{
  ParseNodeYang *other = dynamic_cast <ParseNodeYang *> (ot);

  if (nullptr == other) {
    return false;
  }
  
  return yangnode_->is_conflicting_node(other->yangnode_);
}

bool ParseNodeYang::is_hide_key_keyword ()
{
  RW_ASSERT(yangnode_);

  if (yangnode_->get_stmt_type() != RW_YANG_STMT_TYPE_LIST) {
    // Key keyword suppressed only for List nodes with keys
    return false;
  }

  if (!has_keys()) {
    return false;
  }

  // Check if the list has "show-key-keyword" extenstion option
  if (cli_.skk_adpt_.parse_node_is_cached(this)) {
    return false;
  }

  return true;
}

void ParseNodeYang::get_completions(
  completions_t* completions,
  const std::string& value)
{
  ParseNode::get_completions(completions, value);

  // Specialized completion for handling leaf-list nodes
  if (get_stmt_type() == RW_YANG_STMT_TYPE_LEAF_LIST) {
    // For leaf list show the parent node completions too.
    // Show the completions only when there is atleast one value
    // provided in the leaf list.

    if (children_.size() > 0 && 
        children_.front()->flags_.is_set(ParseFlags::VISITED)) {
      // Create a token_set with the list of visible items. If there is conflicting
      // token, then the token_set.count(token) will return more than 1.
      std::multiset<std::string> token_set;
      for (ParseNode* child : visible_) {
        token_set.insert(child->token_text_get());
      }
  
      for (ParseNode* child : parent_->visible_) {
        if (child == this) {
          // exclude this node, already completed
          continue;
        }

        bool check_prefix = (token_set.count(child->token_text_get()) > 1);
        // Not expecting values
        RW_ASSERT(child->is_keyword());
        if (child->value_is_match(value, check_prefix)) {
          completions->emplace_back(child, check_prefix);
        }
      }
    }
  }

  // For RPCs show the parent's visible children too
  if (is_rpc_input()) {
    bool complete = true;
    if (is_leafy() && !visible_.empty()) {
      // Values to be filled
      complete = false;
    }
    if (get_stmt_type() == RW_YANG_STMT_TYPE_LIST && 
        has_keys() && last_key_child_ != children_.end()) {
      // list and all key fields are not filled
      complete = false;
    }
    if (complete) {
      parent_->get_completions(completions, value);
    }
  }
}

/*****************************************************************************/
/**
 * Create a value parse node.  Necessarily has a parent yang node,
 * parent parse yang node, and a yang value node.
 */
ParseNodeValue::ParseNodeValue(
  BaseCli& cli,
  YangNode* yangnode,
  YangValue* yangvalue,
  ParseNode* parent)
: ParseNode(cli, parent),
  yangnode_(yangnode),
  yangvalue_(yangvalue)
{
  RW_ASSERT(parent_);
  RW_ASSERT(yangnode_);
  RW_ASSERT(yangvalue_);
}

/**
 * Create a value parse node.  Necessarily has a parent yang node,
 * parent parse yang node, and a yang value node.
 */
ParseNodeValue::ParseNodeValue(
  const ParseNodeValue& other,
  ParseNode* parent )
: ParseNode(other, parent),
  yangnode_(other.yangnode_),
  yangvalue_(other.yangvalue_)
{
  RW_ASSERT(parent_);
}

/**
 * Clone a yang value node.  This function provides a strong exception
 * guarantee - if this function throws, the new node is deleted,
 * new_parent is left in a consistent state, and nothing leaks.
 */
ParseNode::ptr_t ParseNodeValue::clone(ParseNode* new_parent) const
{
  RW_ASSERT(NULL == new_parent || &cli_ == &new_parent->cli_);
  ptr_t new_clone(new ParseNodeValue(*this,new_parent));
  return new_clone;
}

/**
 * Check if another node is a clone of me.  Uses both the yangvalue and
 * yangnode pointers for the comparison.
 */
bool ParseNodeValue::is_clone(const ParseNode& other) const
{
  if (this == &other) {
    // Cannot be a clone of self.
    return false;
  }
  clone_visit_t cv(this);
  return other.is_clone_visit(&cv, sizeof(cv));
}

/**
 * Check if another node is a clone of me.
 */
bool ParseNodeValue::is_clone_visit(const void* ocv, size_t size) const
{
  if (size != sizeof(clone_visit_t)) {
    return false;
  }
  clone_visit_t cv(this);
  return 0 == memcmp(&cv,ocv,size);
}

/**
 * Values are leafy by definition.
 */
bool ParseNodeValue::is_leafy() const
{
  return true;
}

/**
 * The value is a key if its parent is a key.
 * ATTN: Should key values be is_key()==false, and is_keyish()==true?
 *   I feel like there should be a difference between the actual key
 *   leaf, and the key leaf's value.
 */
bool ParseNodeValue::is_key() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_key();
}

/**
 * Determine if a command is complete, from this node down.  Command is
 * complete if a value has been set.
 */
bool ParseNodeValue::is_sentence() const
{
  // Cannot be complete if never visted this node.
  if (! flags_.is_set(ParseFlags::VISITED)) {
    return false;
  }

  // Cannot be complete if never visted this node.
  return flags_.is_set(ParseFlags::HAS_VALUE);
}

/**
 * The value is config if its parent is config.
 */
bool ParseNodeValue::is_config() const
{
  RW_ASSERT(yangnode_);
  return yangnode_->is_config();
}

/**
 * Yes, a value is a value. Duh.
 */
bool ParseNodeValue::is_value() const
{
  return true;
}

/**
 * Value nodes can't have descendant keys.
 */
bool ParseNodeValue::has_keys() const
{
  return false;
}

/**
 * Value nodes are not mode nodes.
 * ATTN: But maybe they are for:
 *  - anyxml (multi-line xml)
 *  - binary (multi-line base64)
 *  - optionally fo string, based on rift extension (multi-line string)
 */
bool ParseNodeValue::is_mode() const
{
  return false;
}

/**
 * Schema nodes are always keywords.
 */
bool ParseNodeValue::is_keyword() const
{
  RW_ASSERT(yangvalue_);
  return yangvalue_->is_keyword();
}

/**
 * Get a string that represents the keyword, or value type, that can be
 * used for identifying and sorting keywords in the CLI for help.
 */
const char* ParseNodeValue::token_text_get() const
{
  RW_ASSERT(yangvalue_);
  return yangvalue_->get_name();
}

/**
 * Get a short help string.  The short help string is used in the CLI
 * '?' completion list, to summarize each command or value.  It should
 * not contain embedded newlines.
 */
const char* ParseNodeValue::help_short_get() const
{
  RW_ASSERT(yangvalue_);
  if (!is_key()) {
    return yangvalue_->get_help_short();
  } else {
    return yangnode_->get_help_short();
  }
}

/**
 * Get a full help string.  The full help string is used by the CLI
 * help command, or when displaying help with a shell command line
 * argument.  It may contain embedded newlines.
 */
const char* ParseNodeValue::help_full_get() const
{
  RW_ASSERT(yangvalue_);
  if (!is_key()) {
    return yangvalue_->get_help_full();
  } else {
    return yangnode_->get_help_full();
  }
}

/**
  * Get the prefix of the underlying YANG node
 */
const char* ParseNodeValue::get_prefix() {
  RW_ASSERT (yangvalue_);
  return (yangvalue_->get_prefix());
}

/**
 * Get the namespace of the underlying YANG node
 */
const char* ParseNodeValue::get_ns() {
  RW_ASSERT (yangvalue_);
  return (yangvalue_->get_ns());
}

rw_yang_leaf_type_t ParseNodeValue::get_leaf_type() const
{
  RW_ASSERT (yangvalue_);
  return yangvalue_->get_leaf_type();
}


/**
 * Attempt to match a string value to the node.  If a keyword, a prefix
 * matches.  If a parsed value, then the underlying yang model must
 * parse the value to determine if it is good.
 */
bool ParseNodeValue::value_is_match(
        const std::string& value,
        bool check_prefix) const
{
  (void)check_prefix;
  rw_status_t rs = yangvalue_->parse_partial(value.c_str());
  return rs == RW_STATUS_SUCCESS;
}

/**
 * Try to complete a value.  If the value is not a keyword, it will be
 * taken as is.  Assumes that the value matches; no checks are
 * performed.
 */
const std::string& ParseNodeValue::value_complete(const std::string& value)
{
  // ATTN: In debug build, validate that the new value actually matches?
  if (yangvalue_->is_keyword()) {
    value_set(yangvalue_->get_name());
    return value_;
  }

  if (value.length() > 0) {
    std::string completed_word(value);
    value_set(completed_word);
    return value_;
  }

  value_set(value);
  return value_;
}

/**
 * Determine the root node for parsing the next word in the line
 * buffer.  Depending on the context, it could be the current node, or
 * one of the node's ancestors.
 */
void ParseNodeValue::next(ParseLineResult* plr)
{
  flags_.set(ParseFlags::VISITED);

  /*
   * Cases:
   *  - parent = anyxml:
   *     - ATTN: Not implemented.
   *  - parent = leaf-list:
   *     - ATTN: Could keep track of the number of entries in parser state...
   *     - ATTN: Does it make any sense to enter and leave a leaf-list in a single command line?
   *     - Keep me and my parent - continue to accept more entries in the list
   *  - parent = leaf:
   *     - INT8, INT16, INT32, INT64,
   *       UINT8, UINT16, UINT32, UINT64,
   *       DECIMAL64, BOOLEAN, ENUM, STRING, IDREF:
   *        - No more values can be accepted.  Consume me and my prent.
   *     - BINARY:
   *        - ATTN: Not sure how this will parse, nor if it requires a special mode.
   *        - No more values can be accepted.  Consume me and my parent.
   *     - LEAFREF, INSTANCE_ID:
   *        - ATTN: These need validation against the DOM.
   *        - No more values can be accepted.  Consume me and my parent.
   *     - BITS:
   *        - ATTN: Not yet supported
   *        - Remove all value siblings except bits in my domain
   *     - EMPTY, ANYXML:
   *        - Shouldn't be possible to get here with parent as leaf.
   */
  rw_yang_stmt_type_t const stmt_type = yangnode_->get_stmt_type();

  RW_ASSERT(parent_);
  RW_ASSERT(plr->result_node_->parent_);

  if (RW_YANG_STMT_TYPE_LEAF_LIST == stmt_type) {
    // Next node is my parent (i.e., continue entering items into the list).
    // ATTN: in the future, this is where we would count list length.
    plr->result_node_ = plr->result_node_->parent_;
    plr->parse_node_ = parent_;
    parent_->next(plr);
    return;
  }

  RW_ASSERT(RW_YANG_STMT_TYPE_ANYXML != stmt_type); // ATTN: Not yet

  RW_ASSERT(RW_YANG_STMT_TYPE_LEAF == stmt_type);
  rw_yang_leaf_type_t const leaf_type = yangvalue_->get_leaf_type();

  switch (leaf_type) {
    case RW_YANG_LEAF_TYPE_INT8:
    case RW_YANG_LEAF_TYPE_INT16:
    case RW_YANG_LEAF_TYPE_INT32:
    case RW_YANG_LEAF_TYPE_INT64:
    case RW_YANG_LEAF_TYPE_UINT8:
    case RW_YANG_LEAF_TYPE_UINT16:
    case RW_YANG_LEAF_TYPE_UINT32:
    case RW_YANG_LEAF_TYPE_UINT64:
    case RW_YANG_LEAF_TYPE_DECIMAL64:
    case RW_YANG_LEAF_TYPE_BOOLEAN:
    case RW_YANG_LEAF_TYPE_IDREF:
    case RW_YANG_LEAF_TYPE_STRING:
    case RW_YANG_LEAF_TYPE_ENUM:
    case RW_YANG_LEAF_TYPE_BINARY: // ATTN: may be special in the future
    case RW_YANG_LEAF_TYPE_LEAFREF: // ATTN: may be special in the future
    case RW_YANG_LEAF_TYPE_INSTANCE_ID: // ATTN: may be special in the future
      plr->result_node_ = plr->result_node_->parent_;
      plr->parse_node_ = parent_;
      parent_->next_hide_all(plr);
      return;

    case RW_YANG_LEAF_TYPE_BITS:
      // ATTN: What if there are more bits: this is not implemented.
      // ATTN: union support: Delete all my sibling values that are not from my bits type
      // Consume me, but keep accepting more bits.
      plr->result_node_ = plr->result_node_->parent_;
      plr->parse_node_ = parent_;
      parent_->next_hide(this, plr);
      return;

    case RW_YANG_LEAF_TYPE_EMPTY:
      RW_ASSERT_NOT_REACHED(); // empty leaf should not have value child

    case RW_YANG_LEAF_TYPE_ANYXML:
      RW_ASSERT_NOT_REACHED(); // should have been statement type anyxml

    default:
      break;
  }
  RW_ASSERT_NOT_REACHED(); // bogus value
}



/*****************************************************************************/
/**
 * Create a built-in parse node with the specified parent node (which
 * may be NULL).
 */
ParseNodeBuiltin::ParseNodeBuiltin(BaseCli& cli, const char* token_text, ParseNode* parent)
: ParseNode(cli, parent),
  token_text_(token_text),
  is_leafy_(false),
  is_config_(false),
  is_mode_(false),
  is_cli_print_hook_(false)
{
}

/**
 * Copy construct a new built-in parse node from another node, setting
 * the parent.  The parent may be NULL.
 */
ParseNodeBuiltin::ParseNodeBuiltin(const ParseNodeBuiltin& other, ParseNode* new_parent)
: ParseNode(other, new_parent),
  token_text_(other.token_text_),
  help_short_(other.help_short_),
  help_full_(other.help_full_),
  is_leafy_(other.is_leafy_),
  is_config_(other.is_config_),
  is_mode_(other.is_mode_),
  is_cli_print_hook_(false)
{
}

/**
 * Clone a built-in node.  This function provides a strong exception
 * guarantee - if this function throws, the new node (tree) is deleted,
 * new_parent is left in a consistent state, and nothing leaks.
 */
ParseNode::ptr_t ParseNodeBuiltin::clone(ParseNode* new_parent) const
{
  RW_ASSERT(NULL == new_parent || &cli_ == &new_parent->cli_);
  ptr_t new_clone(new ParseNodeBuiltin(*this,new_parent));
  return new_clone;
}

/**
 * Determine if a built-in is mode-entering.  Only if it is configured
 * as a mode.
 */
ParseNode* ParseNodeBuiltin::mode_enter_top()
{
  if (is_mode_) {
    return this;
  }
  return NULL;
}

/**
 * Check if another node is a clone of me.  Uses token text for the
 * comparison.
 */
bool ParseNodeBuiltin::is_clone(const ParseNode& other) const
{
  if (this == &other) {
    // Cannot be a clone of self.
    return false;
  }
  clone_visit_t cv(this);
  return other.is_clone_visit(&cv, sizeof(cv));
}

/**
 * Check if another node is a clone of me.
 */
bool ParseNodeBuiltin::is_clone_visit(const void* ocv, size_t size) const
{
  if (size != sizeof(clone_visit_t)) {
    return false;
  }
  clone_visit_t cv(this);
  return 0 == memcmp(&cv,ocv,size);
}

/**
 * Get the token text.
 */
const char* ParseNodeBuiltin::token_text_get() const
{
  return token_text_;
}

/**
 * Get a short help string.
 */
const char* ParseNodeBuiltin::help_short_get() const
{
  return help_short_.c_str();
}

/**
 * Get a full help string.
 */
const char* ParseNodeBuiltin::help_full_get() const
{
  return help_full_.c_str();
}

/**
 * Determine the root node for parsing the next word in the line
 * buffer.  This function may have been called directly from the line
 * parser, of from a child node that decided that it is has been fully
 * consumed.
 */
void ParseNodeBuiltin::next(ParseLineResult* plr)
{
  flags_.set(ParseFlags::VISITED);
  if (flags_.is_set(ParseFlags::PARSE_TOP)) {
    return;
  }

  // Stay here is no parent.
  if (!parent_) {
    return;
  }

  RW_ASSERT(parent_);
  RW_ASSERT(plr->result_node_->parent_);
  plr->result_node_ = plr->result_node_->parent_;
  plr->parse_node_ = parent_;
  return;
}

/**
 * Set a short help string.
 */
void ParseNodeBuiltin::help_short_set(const char* str)
{
  help_short_ = str;
}

/**
 * Set a full help string.
 */
void ParseNodeBuiltin::help_full_set(const char* str)
{
  help_full_ = str;
}



/*****************************************************************************/
ParseNodeFunctional::ParseNodeFunctional(BaseCli& cli, const char* token_text, ParseNode* parent)
    : ParseNode(cli, parent),
      token_text_(token_text),
      is_leafy_(false),
      is_config_(false),
      is_mode_(false),
      is_sentence_ (false),
      cb_ (nullptr)
{

}

/**
 * Copy construct a new built-in parse node from another node, setting
 * the parent.  The parent may be NULL.
 */
ParseNodeFunctional::ParseNodeFunctional(BaseCli& cli, const ParseNodeFunctional& other, ParseNode* new_parent)
: ParseNode(cli, new_parent),
  token_text_(other.token_text_),
  help_short_(other.help_short_),
  help_full_(other.help_full_),
  is_leafy_(other.is_leafy_),
  is_config_(other.is_config_),
  is_mode_(other.is_mode_),
  cb_ (other.cb_),
  cli_print_hook_ (other.cli_print_hook_)
{
  flags_ = other.flags_;
    // clone descendents
  for (desc_citer_t citer = other.descendents_.cbegin(); citer != other.descendents_.cend(); citer++) {
    d_ptr_t temp (new ParseNodeFunctional (*citer->get(), 0));
    descendents_.push_back (std::move (temp));
  }
}

/**
 * Copy construct a new built-in parse node from another node, setting
 * the parent.  The parent may be NULL.
 */
ParseNodeFunctional::ParseNodeFunctional(const ParseNodeFunctional& other, ParseNode* new_parent)
: ParseNode(other, new_parent),
  token_text_(other.token_text_),
  help_short_(other.help_short_),
  help_full_(other.help_full_),
  is_leafy_(other.is_leafy_),
  is_config_(other.is_config_),
  is_mode_(other.is_mode_),
  cb_ (other.cb_),
  cli_print_hook_ (other.cli_print_hook_)
{
  flags_ = other.flags_;

  // clone descendents
  for (desc_citer_t citer = other.descendents_.cbegin(); citer != other.descendents_.cend(); citer++) {
    d_ptr_t temp (new ParseNodeFunctional (*citer->get(), this));
    descendents_.push_back (std::move (temp));
  }

  if (nullptr != other.value_node_.get()) {
    v_ptr_t temp1 (new ParseNodeValueInternal (*(other.value_node_.get()), this));
    value_node_ = (std::move (temp1));
  }
}

ParseNodeFunctional::~ParseNodeFunctional()
{
}

/**
 * Clone a built-in node.  This function provides a strong exception
 * guarantee - if this function throws, the new node (tree) is deleted,
 * new_parent is left in a consistent state, and nothing leaks.
 */
ParseNode::ptr_t ParseNodeFunctional::clone(ParseNode* new_parent) const
{
  RW_ASSERT(NULL == new_parent || &cli_ == &new_parent->cli_);
  ptr_t new_clone(new ParseNodeFunctional(*this,new_parent));
  return new_clone;
}

/**
 * Functional nodes are not mode entering (for now). Behavioral nodes could be
 * as a mode.
 */
ParseNode* ParseNodeFunctional::mode_enter_top()
{
  return NULL;
}

/**
 * Check if another node is a clone of me.  Uses token text for the
 * comparison.
 */
bool ParseNodeFunctional::is_clone(const ParseNode& other) const
{
  if (this == &other) {
    // Cannot be a clone of self.
    return false;
  }
  clone_visit_t cv(this);
  return other.is_clone_visit(&cv, sizeof(cv));
}

/**
 * Check if another node is a clone of me.
 */
bool ParseNodeFunctional::is_clone_visit(const void* ocv, size_t size) const
{
  if (size != sizeof(clone_visit_t)) {
    return false;
  }
  clone_visit_t cv(this);
  return 0 == memcmp(&cv,ocv,size);
}

/**
 * Get the token text.
 */
const char* ParseNodeFunctional::token_text_get() const
{
  return token_text_;
}

/**
 * Get a short help string.
 */
const char* ParseNodeFunctional::help_short_get() const
{
  return help_short_.c_str();
}

/**
 * Get a full help string.
 */
const char* ParseNodeFunctional::help_full_get() const
{
  return help_full_.c_str();
}

/**
 * Set a short help string.
 */
void ParseNodeFunctional::help_short_set(const char* str)
{
  help_short_ = str;
}

/**
 * Set a full help string.
 */
void ParseNodeFunctional::help_full_set(const char* str)
{
  help_full_ = str;
}

const char* ParseNodeFunctional::get_cli_print_hook_string() {
  if (cli_print_hook_.length()) {
    return cli_print_hook_.c_str();
  }
  return nullptr;
}


void ParseNodeFunctional::set_cli_print_hook(const char *plugin_name)
{
  if (nullptr != plugin_name) {
    cli_print_hook_ = plugin_name;
  }
  else {
    cli_print_hook_.clear();
  }
}

bool ParseNodeFunctional::is_cli_print_hook() const
{
  return (cli_print_hook_.length() != 0);
}

/**
 * Functional nodes children are filled from the descendents
 * to the root node of the current mode.
 */

void ParseNodeFunctional::fill_children ()
{

  if (flags_.is_set(ParseFlags::C_FILLED)) {
    return;
  }

  children_.clear();
  visible_.clear();

  for (desc_citer_t citer = descendents_.cbegin(); citer != descendents_.cend(); citer++) {
    ptr_t temp (new ParseNodeFunctional (*citer->get(), this));
    children_.push_back (std::move (temp));
  }

  if (nullptr != value_node_.get()) {
    v_ptr_t temp (new ParseNodeValueInternal (*value_node_.get(), this));
    children_.push_back (std::move (temp));
  }

  flags_.set(ParseFlags::C_FILLED);
}

/**
 * Determine the root node for parsing the next word in the line
 * buffer.  This function may have been called directly from the line
 * parser, of from a child node that decided that it is has been fully
 * consumed.
 */
void ParseNodeFunctional::next(ParseLineResult* plr)
{
  flags_.set(ParseFlags::VISITED);

  // The behavior nodes children need to be reset everytime the node is visited,
  // since walks up and down the "modes" change the children of these nodes
  if (flags_.is_clear(ParseFlags::C_FILLED)) {
    fill_children();
  } else {
    visible_.clear();
  }

  for (child_iter_t ci = children_.begin(); ci != children_.end(); ++ci) {
    // the functionals children are alway "suitable"
    visible_.insert(ci->get());
  }
  return;
}



/**
 * A ParseNodeFunctional is a sentence if at least one visible node has been visited.
 */
bool ParseNodeFunctional::is_sentence() const
{

  // Cannot be complete if never visted this node.
  if (! flags_.is_set(ParseFlags::VISITED)) {
    return false;
  }

  if (!children_.size()) {
    return true;
  }

  bool visited = false;

  for (child_citer_t vi = children_.cbegin(); vi != children_.cend(); ++vi) {
    if ((*vi)->flags_.is_set(ParseFlags::VISITED)) {

      if (flags_.is_set (ParseFlags::HIDE_VALUES)) {
        // any child node visited should complete such commands
        return true;
      }

      if (! (*vi)->is_sentence()) {
        return false;
      }
      visited = true;
    }
  }

  return visited;
}

/**
 * Set a callback function to be called when a functional node completes processing
 */

void ParseNodeFunctional::set_callback (CliCallback *cb)
{
  cb_ = cb;
}

/**
 * Add a new descendent to the current node
 */
void ParseNodeFunctional::add_descendent (ParseNodeFunctional *desc)
{
  d_ptr_t temp (new ParseNodeFunctional (*desc, this));
  descendents_.push_back (std::move (temp));
}

ParseNodeFunctional* ParseNodeFunctional::find_descendent(const std::string& token)
{
  for (auto& desc: descendents_) {
    if (token == desc->token_text_get()) {
      return desc.get();
    }
  }

  return nullptr;
}

ParseNodeFunctional* ParseNodeFunctional::find_descendent_deep(
                          const std::vector<std::string>& path)
{
  // An empty path will return this ptr
  ParseNodeFunctional* result = this;
  for(auto& token: path) {
    result = result->find_descendent(token);
    if (result == nullptr) {
      return result;
    }
  }
  return result;
}

/**
 * Get the call back function associated with this parse node
 */
CliCallback* ParseNodeFunctional::get_callback()
{
  return cb_;
}

/**
 * Set a value node that can be used to specify arguments to a CLI command
 */
void ParseNodeFunctional::set_value_node (ParseNodeValueInternal *value_node)
{
  v_ptr_t temp (new ParseNodeValueInternal (*value_node, this));
  value_node_ = (std::move (temp));
}



/****************************************************************************************/
/**
 *
 */
ParseNodeBehavior::ParseNodeBehavior(BaseCli& cli, const char* token_text, ParseNode* parent)
    : ParseNodeFunctional(cli, token_text, parent)
{

}

/**
 * Copy construct a new built-in parse node from another node, setting
 * the parent.  The parent may be NULL.
 */
ParseNodeBehavior::ParseNodeBehavior(BaseCli& cli, const ParseNodeBehavior& other, ParseNode* new_parent)
    : ParseNodeFunctional(cli, other,  new_parent)
{
  flags_ = other.flags_;
}

/**
 * Copy construct a new built-in parse node from another node, setting
 * the parent.  The parent may be NULL.
 */
ParseNodeBehavior::ParseNodeBehavior(const ParseNodeBehavior& other, ParseNode* new_parent)
: ParseNodeFunctional(other, new_parent)
{
}

/**
 * Clone a built-in node.  This function provides a strong exception
 * guarantee - if this function throws, the new node (tree) is deleted,
 * new_parent is left in a consistent state, and nothing leaks.
 */
ParseNode::ptr_t ParseNodeBehavior::clone(ParseNode* new_parent) const
{
  RW_ASSERT(NULL == new_parent || &cli_ == &new_parent->cli_);
  ptr_t new_clone(new ParseNodeBehavior(*this,new_parent));
  return new_clone;
}

/**
 * Determine if a behavior is mode-entering.  Only if it is configured
 * as a mode.
 */
ParseNode* ParseNodeBehavior::mode_enter_top()
{
  if (is_mode_) {
    // ATTN: This assumes that behavior nodes that enter modes do so at the top of the
    // current mode, and the current parse yang node has not been "processed"
    // This also assumes that once the config node is returned, the config mode is
    // always entered.

    // If this turns out to be an issue, maybe each behavioral should register an mode-enter
    // action function that is then exectued before that mode is entered.

    RW_ASSERT(cli_.current_mode_);
    RW_ASSERT(cli_.current_mode_->top_parse_node_);
    RW_ASSERT (BaseCli::STATE_CONFIG != cli_.state_);

    return this;
  }
  return NULL;
}

/**
 * Check if another node is a clone of me.  Uses token text for the
 * comparison.
 */
bool ParseNodeBehavior::is_clone(const ParseNode& other) const
{
  if (this == &other) {
    // Cannot be a clone of self.
    return false;
  }
  clone_visit_t cv(this);
  return other.is_clone_visit(&cv, sizeof(cv));
}

/**
 * Check if another node is a clone of me.
 */
bool ParseNodeBehavior::is_clone_visit(const void* ocv, size_t size) const
{
  if (size != sizeof(clone_visit_t)) {
    return false;
  }
  clone_visit_t cv(this);
  return 0 == memcmp(&cv,ocv,size);
}

/**
 * Get the token text.
 */
const char* ParseNodeBehavior::token_text_get() const
{
  return token_text_;
}

/**
 * Get a short help string.
 */
const char* ParseNodeBehavior::help_short_get() const
{
  return help_short_.c_str();
}

/**
 * Get a full help string.
 */
const char* ParseNodeBehavior::help_full_get() const
{
  return help_full_.c_str();
}


/**
 * Behavior nodes children could be filled from the yang node that corresponds
 * to the root node of the current mode.
 */

void ParseNodeBehavior::fill_children (YangNode *yangnode)
{
  RW_ASSERT (!yangnode->is_leafy());

  if (flags_.is_set(ParseFlags::C_FILLED)) {
    return;
  }

  visible_.clear();

  for (YangNodeIter yni = yangnode->child_begin(); yni != yangnode->child_end(); ++yni) {
    ptr_t new_node(new ParseNodeYang(cli_,&*yni,this));
    children_.push_back(std::move(new_node));
  }

  if (cli_.state_ == BaseCli::STATE_CONFIG && 
      flags_.is_set(ParseFlags::GET_GENERIC)) {
    // For show config, add config and candidate config as children
    ParseNodeBehavior *tmp = static_cast<ParseNodeBehavior*>(
                                cli_.show_config_node_.get());
    ptr_t conf_node(new ParseNodeBehavior(cli_, *tmp, this));
    children_.push_back(std::move(conf_node));

    tmp = static_cast<ParseNodeBehavior*>(cli_.show_candidate_node_.get());
    if (tmp) {
      ptr_t cand_node(new ParseNodeBehavior(cli_, *tmp, this));
      children_.push_back(std::move(cand_node));
    }
  }

  flags_.set(ParseFlags::C_FILLED);
}

/**
 * Determine the root node for parsing the next word in the line
 * buffer.  This function may have been called directly from the line
 * parser, or from a child node that decided that it is has been fully
 * consumed.
 */
void ParseNodeBehavior::next(ParseLineResult* plr)
{
  flags_.set(ParseFlags::VISITED);

  // The behavior nodes children need to be reset everytime the node is visited,
  // since walks up and down the "modes" change the children of these nodes
  flags_.clear(ParseFlags::C_FILLED);
  if (flags_.is_set(ParseFlags::PARSE_TOP)) {
    return;
  }

  if (is_mode_) {
    // this is the place to be. no further inputs accepted
    return;
  }

  // for all other behavior nodes, the completions are the ones in the mode root
  // but with flags set to those of this node.

  // remove any previously visible nodes
  visible_.clear();
  RW_ASSERT(cli_.current_mode_);
  RW_ASSERT(cli_.current_mode_->top_parse_node_);

  ParseNodeFunctional::fill_children();
  flags_.clear(ParseFlags::C_FILLED);
  ParseNodeYang *mode_root = dynamic_cast <ParseNodeYang *> (cli_.current_mode_->top_parse_node_);
  fill_children(mode_root->yangnode_);

  for (child_iter_t ci = children_.begin(); ci != children_.end(); ++ci) {
    if ((*ci)->is_suitable (flags_)) {
      visible_.insert(ci->get());
    }
  }
  return;
}


/*****************************************************************************/
/**
 * Create a internal value parse node.  Necessarily has a parent functional node
 */
ParseNodeValueInternal::ParseNodeValueInternal(
    BaseCli& cli,
    rw_yang_leaf_type_t type,
    ParseNode* parent)
    :ParseNode (cli, parent),
    type_ (type),
    token_text_(nullptr),
    is_leafy_(false),
    is_config_(false),
    is_mode_(false)
{
  RW_ASSERT(rw_yang_leaf_type_is_good(type));
}

/**
 * Create a internal value parse node.
 */
ParseNodeValueInternal::ParseNodeValueInternal(
    const ParseNodeValueInternal& other,
    ParseNode* parent )
: ParseNode(other, parent),
  type_(other.type_),
  token_text_(other.token_text_),
  help_short_(other.help_short_),
  help_full_(other.help_full_),
  is_leafy_(other.is_leafy_),
  is_config_(other.is_config_),
  is_mode_(other.is_mode_)
{
  RW_ASSERT(parent_);
}

/**
 * Clone a internal value node.  This function provides a strong exception
 * guarantee - if this function throws, the new node is deleted,
 * new_parent is left in a consistent state, and nothing leaks.
 */
ParseNode::ptr_t ParseNodeValueInternal::clone(ParseNode* new_parent) const
{
  RW_ASSERT(NULL == new_parent || &cli_ == &new_parent->cli_);
  ptr_t new_clone(new ParseNodeValueInternal(*this,new_parent));
  return new_clone;
}

/**
 * Check if another node is a clone of me.  Uses the type for comparision.
 */
bool ParseNodeValueInternal::is_clone(const ParseNode& other) const
{
  if (this == &other) {
    // Cannot be a clone of self.
    return false;
  }
  clone_visit_t cv(this);
  return other.is_clone_visit(&cv, sizeof(cv));
}

/**
 * Check if another node is a clone of me.
 */
bool ParseNodeValueInternal::is_clone_visit(const void* ocv, size_t size) const
{
  if (size != sizeof(clone_visit_t)) {
    return false;
  }
  clone_visit_t cv(this);
  return 0 == memcmp(&cv,ocv,size);
}

/**
 * Values are leafy by definition.
 */
bool ParseNodeValueInternal::is_leafy() const
{
  return true;
}

/**
 * Internal values are never keys
 */
bool ParseNodeValueInternal::is_key() const
{
  return false;
}

/**
 * Determine if a command is complete, from this node down.  Command is
 * complete if a value has been set.
 */
bool ParseNodeValueInternal::is_sentence() const
{
  // Cannot be complete if never visted this node.
  if (! flags_.is_set(ParseFlags::VISITED)) {
    return false;
  }

  // Cannot be complete if value was not set
  return flags_.is_set(ParseFlags::HAS_VALUE);
}

/**
 * Yes, a value is a value. Duh.
 */
bool ParseNodeValueInternal::is_value() const
{
  return true;
}

/**
 * is this configuration?
 */
bool ParseNodeValueInternal::is_config() const
{
  return is_config_;
}

/**
 * Value nodes can't have descendant keys.
 */
bool ParseNodeValueInternal::has_keys() const
{
  return false;
}

/**
 * Internal nodes are never modes
 */
bool ParseNodeValueInternal::is_mode() const
{
  return false;
}

/**
 * Internal values are not keywords
 */
bool ParseNodeValueInternal::is_keyword() const
{
  return false;
}

/**
 * Get a string that represents the keyword, or value type, that can be
 * used for identifying and sorting keywords in the CLI for help.
 */
const char* ParseNodeValueInternal::token_text_get() const
{
  RW_ASSERT(rw_yang_leaf_type_is_good(type_));
  return rw_yang_leaf_type_string(type_);
}

/**
 * Get a short help string.  The short help string is used in the CLI
 * '?' completion list, to summarize each command or value.  It should
 * not contain embedded newlines.
 */
const char* ParseNodeValueInternal::help_short_get() const
{
  return help_short_.c_str();
}

/**
 * Get a full help string.  The full help string is used by the CLI
 * help command, or when displaying help with a shell command line
 * argument.  It may contain embedded newlines.
 */
const char* ParseNodeValueInternal::help_full_get() const
{
  return help_full_.c_str();
}

/**
 * Set a short help string.
 */
void ParseNodeValueInternal::help_short_set(const char* str)
{
  help_short_ = str;
}

/**
 * Set a full help string.
 */
void ParseNodeValueInternal::help_full_set(const char* str)
{
  help_full_ = str;
}


/**
 * Attempt to match a string value to the node.
 * always true - for now
 */
bool ParseNodeValueInternal::value_is_match(
        const std::string& value,
        bool check_prefix) const
{
  // ATTN: this needs to change to correctly identify any issues in the values
  return true;
}

/**
 * Try to complete a value.  If the value is not a keyword, it will be
 * taken as is.  Assumes that the value matches; no checks are
 * performed.
 */
const std::string& ParseNodeValueInternal::value_complete(const std::string& value)
{

  if (value.length() > 0) {
    std::string completed_word(value);
    value_set(completed_word);
    return value_;
  }

  value_set(value);
  return value_;
}

/**
 * Determine the root node for parsing the next word in the line
 * buffer.  Depending on the context, it could be the current node, or
 * one of the node's ancestors.
 */
void ParseNodeValueInternal::next(ParseLineResult* plr)
{
  flags_.set(ParseFlags::VISITED);

  /*
   * Cases:
   *  - parent = anyxml:
   *     - ATTN: Not implemented.
   *  - parent = leaf-list:
   *  - parent = leaf:
   *     - INT8, INT16, INT32, INT64,
   *       UINT8, UINT16, UINT32, UINT64,
   *       DECIMAL64, BOOLEAN, ENUM, STRING, IDREF:
   *        - No more values can be accepted.  Consume me and my prent.
   *     - BINARY:
   */

  switch (type_) {
    case RW_YANG_LEAF_TYPE_INT8:
    case RW_YANG_LEAF_TYPE_INT16:
    case RW_YANG_LEAF_TYPE_INT32:
    case RW_YANG_LEAF_TYPE_INT64:
    case RW_YANG_LEAF_TYPE_UINT8:
    case RW_YANG_LEAF_TYPE_UINT16:
    case RW_YANG_LEAF_TYPE_UINT32:
    case RW_YANG_LEAF_TYPE_UINT64:
    case RW_YANG_LEAF_TYPE_DECIMAL64:
    case RW_YANG_LEAF_TYPE_BOOLEAN:
    case RW_YANG_LEAF_TYPE_IDREF:
    case RW_YANG_LEAF_TYPE_STRING:
    case RW_YANG_LEAF_TYPE_ENUM:
    case RW_YANG_LEAF_TYPE_BINARY: // ATTN: may be special in the future
    case RW_YANG_LEAF_TYPE_LEAFREF: // ATTN: may be special in the future
    case RW_YANG_LEAF_TYPE_INSTANCE_ID: // ATTN: may be special in the future
      plr->result_node_ = plr->result_node_->parent_;
      plr->parse_node_ = parent_;
      parent_->next_hide_all(plr);
      return;

    default:
      break;
  }
  RW_ASSERT_NOT_REACHED(); // bogus value
}


ParseNodeValueWildcard::ParseNodeValueWildcard(
        BaseCli& cli,
        ParseNode* parent)
  : ParseNode (cli, parent)
{
  RW_ASSERT(parent_);
  help_str_ = std::string("any ") + parent_->token_text_get();
}

/**
 * Create a internal value parse node.
 */
ParseNodeValueWildcard::ParseNodeValueWildcard(
    const ParseNodeValueWildcard& other,
    ParseNode* parent)
  : ParseNode(other, parent)
{
  RW_ASSERT(parent_);
  help_str_ = std::string("any ") + parent_->token_text_get();
}

/**
 * Clone a internal value node.  This function provides a strong exception
 * guarantee - if this function throws, the new node is deleted,
 * new_parent is left in a consistent state, and nothing leaks.
 */
ParseNode::ptr_t ParseNodeValueWildcard::clone(ParseNode* new_parent) const
{
  RW_ASSERT(NULL == new_parent || &cli_ == &new_parent->cli_);
  ptr_t new_clone(new ParseNodeValueWildcard(*this,new_parent));
  return new_clone;
}

/**
 * Check if another node is a clone of me.  Uses the type for comparision.
 */
bool ParseNodeValueWildcard::is_clone(const ParseNode& other) const
{
  if (this == &other) {
    // Cannot be a clone of self.
    return false;
  }
  clone_visit_t cv(this);
  return other.is_clone_visit(&cv, sizeof(cv));
}

/**
 * Check if another node is a clone of me.
 */
bool ParseNodeValueWildcard::is_clone_visit(const void* ocv, size_t size) const
{
  if (size != sizeof(clone_visit_t)) {
    return false;
  }
  clone_visit_t cv(this);
  return 0 == memcmp(&cv,ocv,size);
}

/**
 * Values are leafy by definition.
 */
bool ParseNodeValueWildcard::is_leafy() const
{
  return true;
}

/**
 * 
 */
bool ParseNodeValueWildcard::is_key() const
{
  return parent_->is_key();
}

/**
 * Determine if a command is complete, from this node down.  Command is
 * complete if a value has been set.
 */
bool ParseNodeValueWildcard::is_sentence() const
{
  // Cannot be complete if never visted this node.
  if (! flags_.is_set(ParseFlags::VISITED)) {
    return false;
  }

  // Cannot be complete if value was not set
  return flags_.is_set(ParseFlags::HAS_VALUE);
}

/**
 * Yes, a value is a value. Duh.
 */
bool ParseNodeValueWildcard::is_value() const
{
  return true;
}

/**
 * is this configuration?
 */
bool ParseNodeValueWildcard::is_config() const
{
  return parent_->is_config();
}

/**
 * Value nodes can't have descendant keys.
 */
bool ParseNodeValueWildcard::has_keys() const
{
  return false;
}

/**
 * Value nodes are never modes
 */
bool ParseNodeValueWildcard::is_mode() const
{
  return false;
}

/**
 * Value nodes are not keywords
 */
bool ParseNodeValueWildcard::is_keyword() const
{
  return false;
}

/**
 * Get a string that represents the keyword, or value type, that can be
 * used for identifying and sorting keywords in the CLI for help.
 */
const char* ParseNodeValueWildcard::token_text_get() const
{
  return wildcard_str_;
}

/**
 * Get a short help string.  The short help string is used in the CLI
 * '?' completion list, to summarize each command or value.  It should
 * not contain embedded newlines.
 */
const char* ParseNodeValueWildcard::help_short_get() const
{
  return help_str_.c_str();
}

/**
 * Get a full help string.  The full help string is used by the CLI
 * help command, or when displaying help with a shell command line
 * argument.  It may contain embedded newlines.
 */
const char* ParseNodeValueWildcard::help_full_get() const
{
  return help_str_.c_str();
}

/**
 * Attempt to match a string value to the node.
 * always true - for now
 */
bool ParseNodeValueWildcard::value_is_match(
        const std::string& value,
        bool check_prefix) const
{
  // ATTN: this needs to change to correctly identify any issues in the values
  return (value == wildcard_str_);
}

/**
 * Try to complete a value.  If the value is not a keyword, it will be
 * taken as is.  Assumes that the value matches; no checks are
 * performed.
 */
const std::string& ParseNodeValueWildcard::value_complete(const std::string& value)
{
  value_set(value);
  return value_;
}

/**
 * Determine the root node for parsing the next word in the line
 * buffer.  Depending on the context, it could be the current node, or
 * one of the node's ancestors.
 */
void ParseNodeValueWildcard::next(ParseLineResult* plr)
{
  flags_.set(ParseFlags::VISITED);
  plr->result_node_ = plr->result_node_->parent_;
  plr->parse_node_ = parent_;
  parent_->next_hide_all(plr);
  return;
}

/*****************************************************************************/
/**
 * Create the root mode state.  This always exists, and only gets
 * deleted when the CLI instance gets deleted.
 */
ModeState::ModeState(BaseCli& cli,
                     const ParseNode& root_node,
                     XMLNode *dom_node)
: cli_(cli),
  mode_parse_path_(std::move(root_node.clone(NULL))),
  top_parse_node_(mode_parse_path_.get()),
  dom_node_ (dom_node)
{}

/**
 * Create a new mode state from parse line results.
 */
ModeState::ModeState(ParseLineResult* r, XMLNode *dom_node)
: cli_(r->cli_),
  mode_parse_path_(std::move(r->result_tree_)),
  top_parse_node_(r->result_node_),
  dom_node_ (dom_node)
{}

/**
 * Create a new mode state from the essense of parse line results.
 */
ModeState::ModeState(ParseNode::ptr_t&& result_tree,
                     ParseNode* result_node,
                     XMLNode *dom_node)
: cli_(result_tree->cli_),
  mode_parse_path_(std::move(result_tree)),
  top_parse_node_(result_node),
  dom_node_ (dom_node)
{}


void ModeState::set_xml_node(XMLNode *node) {
  dom_node_ = node;
}

XMLNode* ModeState::update_xml_cfg (rw_yang::ParseNode *pn, XMLNode *parent_xml)
{
  // ATTN: XPATH should make this a much smaller function

  bool found_match = false;

  XMLNode *current_xml = nullptr;
  XMLNodeList::uptr_t siblings(parent_xml->get_children());

  for (size_t i = 0; i < siblings->length(); i++) {
    current_xml = siblings->at(i);
    if (!strcmp (current_xml->get_local_name().c_str(), pn->value_.c_str())) {

      // The node name matches - check if the keys match
      found_match = true;

      for (ParseNode::child_citer_t child_it = pn->children_.begin(); pn->children_.end() != child_it; ++child_it) {
        ParseNode *child_pn = child_it->get();
        if (child_pn->is_key()) {

          RW_ASSERT (child_pn->is_leafy());
          RW_ASSERT (RW_YANG_STMT_TYPE_LEAF == child_pn->get_stmt_type())
          RW_ASSERT (child_pn->children_.begin() != child_pn->children_.end());

          // get the key from the key node
          if (NULL == current_xml->find_value(child_pn->value_.c_str(),
                                              child_pn->children_.begin()->get()->value_.c_str(),
                                              nullptr /* ATTN: ns */)) {
            found_match = false;
            break;
          }
        }
      }
      if (found_match) {
        // ATTN: This assumes that all the keys are present in the parse node
        break;
      }
    }
  }
  if (found_match) {
    return current_xml;
  }

  return nullptr;
}


XMLNode * ModeState::update_xml_cfg  (XMLDocument *doc, rw_yang::ParseNode::ptr_t &result_tree)
{
  // Add it to the current dom. start from the root of the parse tree
  ParseNode *pn = result_tree->children_.begin()->get();
  XMLNode *parent_xml = dom_node_;
  XmlResultMergeState::stack_t merge_state;
  XMLNode *mode_xml = nullptr;

  while (NULL != pn) {

    XMLNode *current_xml = nullptr;

    if (!pn->is_functional()) {

      if (pn->is_leafy()) {

        switch (pn->get_stmt_type()) {
          case RW_YANG_STMT_TYPE_LEAF: {
            ParseNodeValue *value_child = nullptr;
            // only one of a type - replace if it is already present
            if (pn->children_.begin() != pn->children_.end()) {
              ParseNode* child = pn->children_.begin()->get();
              RW_ASSERT(child->is_value());
              value_child = static_cast<ParseNodeValue*>(child);
            }

            current_xml = pn->get_xml_node (doc, parent_xml);

            if (value_child) {
              if (value_child->get_leaf_type() == RW_YANG_LEAF_TYPE_IDREF) {
                set_idref_value(current_xml, value_child);
              } else {
                current_xml->set_text_value(value_child->value_.c_str());
              }
            }
            break;
          }

          case RW_YANG_STMT_TYPE_LEAF_LIST: {
            // Duplicate values are not allowed for leaf lists
            for (ParseNode::child_citer_t inner = pn->children_.begin();
                 pn->children_.end() != inner; inner ++) {
              // Add the value of the node to the current xml node
              pn->update_xml_for_leaf_list (doc, parent_xml, inner->get()->value_.c_str());
            }
            break;
          }
          default:
            RW_CRASH();
        }

        // once a leaf is reached and processed, indicate that the stack has to be popped
        pn = nullptr;
      }

      else {
        bool skip_keys = false;

        if (pn->has_keys()){
        // A list - Find or create the right xml node to update, and process the keys
          current_xml = update_xml_cfg (pn, parent_xml);
          if (NULL != current_xml ) {
            skip_keys = true;
          } else  {
            current_xml = pn->create_xml_node (doc, parent_xml);
          }
        }

        else {
          current_xml = pn->get_xml_node (doc, parent_xml);
        }

        RW_ASSERT (current_xml);

        // ATTN: assumes that a new mode entry command will have only the name of the container
        // or name and key of the list being configured

        if (pn->is_mode()) {
          mode_xml = current_xml;
        }

        // process the first child after pushing any of its sibilings into the stack
        ParseNode::child_citer_t result_child = pn->children_.begin();
        ParseNode::child_citer_t iter_end = pn->children_.end();

        // skip any keys if set
        while (skip_keys && (result_child != iter_end) && result_child->get()->is_key()) {
          result_child++;
        }

        if (result_child != iter_end) {

          pn = result_child->get();
          parent_xml = current_xml;
          current_xml = nullptr;

          result_child ++;

          while (result_child != iter_end) {
            if (!(skip_keys && result_child->get()->is_key())) {
              merge_state.push_back(XmlResultMergeState(result_child->get(), parent_xml));
            }
            result_child++;
          }
        }
        else {
          // pop a state to get going
          pn = nullptr;
        }
      }
    }
    else {
      pn = nullptr;
    }

    if (nullptr == pn) {
      // Either the present flag was not set for the present node, or all nodes at current level have been exhausted
      if (merge_state.size()) {
        {
          XmlResultMergeState state = merge_state.back();
          merge_state.pop_back();

          pn = state.pn_;
          parent_xml = state.xn_;
        }
      }
    }
  } // while pn!= nullptr

  if (mode_xml) {
    return mode_xml;
  }

  return parent_xml;
}

XMLNode *ModeState::merge_xml (XMLDocument *doc, XMLNode *parent_xml, ParseNode *pn)
{
  bool is_delete_op = false;
  XmlResultMergeState::stack_t merge_state;

  // Functional/Behavioral nodes are always at the top level, there can be
  // multiple functional nodes, skip those (those are not part of the XML)
  while (pn->is_functional()) {
    if (pn->children_.size()) {
      if (strcmp("no", pn->token_text_get()) == 0) {
        is_delete_op = true;
      }
      pn = pn->children_.begin()->get();
    } else {
      return nullptr;
    }
  }

  ParseNode* last_pn = nullptr;
  XMLNode* current_xml = nullptr;
  while (NULL != pn) {

    current_xml = nullptr;
    last_pn = pn;

      if (pn->is_leafy()) {

        switch (pn->get_stmt_type()) {
          case RW_YANG_STMT_TYPE_LEAF: {
            ParseNodeValue *value_child = nullptr;

            // only one of a type - replace if it is already present
            if (pn->children_.begin() != pn->children_.end()) {
              ParseNode *child = pn->children_.begin()->get();
              RW_ASSERT(child->is_value());

              // For a wildcard no text context 
              // (filtering based on selection node)
              if (child->value_ != "*") {
                value_child = static_cast<ParseNodeValue*>(child);
              }
            }

            current_xml = pn->get_xml_node (doc, parent_xml);
            RW_ASSERT (current_xml);

            if (value_child) {
              if (value_child->get_leaf_type() == RW_YANG_LEAF_TYPE_IDREF) {
                set_idref_value(current_xml, value_child);
              } else {
                current_xml->set_text_value(value_child->value_.c_str());
              }
            }
            break;
          }
          case RW_YANG_STMT_TYPE_LEAF_LIST: {
            // Duplicate values are not allowed for leaf lists

            // For show commands, children maynot be present, but the output
            // XML still needs to contain the node

            if (pn->children_.size()) {
              for (ParseNode::child_citer_t inner = pn->children_.begin();
                   pn->children_.end() != inner; inner ++) {
                // Add the value of the node to the current xml node
                current_xml = pn->update_xml_for_leaf_list (doc, parent_xml, inner->get()->value_.c_str());
              }
            } else {
              current_xml = pn->create_xml_node(doc, parent_xml);
            }
            break;
          }
          default:
            RW_CRASH();
        }

        // once a leaf is reached and processed, indicate that the stack has to be popped
        pn = nullptr;
      }
      else {
        current_xml = pn->create_xml_node(doc, parent_xml);
        RW_ASSERT (current_xml);
        parent_xml = current_xml;

        // process the first child after pushing any of its sibilings into the stack
        ParseNode::child_citer_t result_child = pn->children_.begin();
        ParseNode::child_citer_t iter_end = pn->children_.end();

        if (result_child != iter_end) {

          pn = result_child->get();
          result_child ++;

          while (result_child != iter_end) {
            merge_state.push_back(XmlResultMergeState(result_child->get(), parent_xml));
            result_child++;
          }
        }
        else {
          // pop a state to get going
          pn = nullptr;
        }
      }

      if (nullptr == pn) {
        // Either the present flag was not set for the present node, or all nodes at current level have been exhausted
        // Using a Breadth-first approach for constructing the XML tree
        // The ParseNode traversal however is Depth First
        if (0 != merge_state.size()) {
          XmlResultMergeState state = merge_state.front();
          merge_state.pop_front();

          pn = state.pn_;
          parent_xml = state.xn_;
        }
      }
  } // while pn!= nullptr

  if (is_delete_op && last_pn) {
    RW_ASSERT(current_xml);
    if (last_pn->is_leafy() && last_pn->is_key()) {
      // Key nodes deletion means the list is getting deleted
      current_xml = current_xml->get_parent();
    }
    // Add the delete attribute
    current_xml->set_attribute("operation", "delete", 
                    NETCONF_NS, NETCONF_NS_PREFIX);
  }

  return parent_xml;
}

XMLNode * ModeState::create_xml_node(XMLDocument *doc, XMLNode *parent_xml)
{
  // start from the first child of the top node
  ParseNode *pn = mode_parse_path_->children_.begin()->get();

  if (parent_xml == nullptr) {
    parent_xml = doc->get_root_node();
  }

  return merge_xml (doc, parent_xml, pn);
}

ParseNode::ptr_t ModeState::clone_top_parse_node()
{
  RW_ASSERT(top_parse_node_);
  ParseNode::ptr_t clone = top_parse_node_->clone(top_parse_node_->parent_);
  clone->flags_.set(ParseFlags::PARSE_TOP);
  clone->flags_.set(ParseFlags::V_LIST_KEYS_CLONED);
  return clone;
}

void ModeState::set_idref_value(XMLNode* xml_element, ParseNodeValue* value)
{
  // ATTN: should this be part of rw_xml?
  const char* ns = value->get_ns();
  const char* prefix = value->get_prefix();

  if (xml_element->get_name_space() == ns) {
    // The Element and the value in the same namespace, use the prefix of the
    // element
    prefix = RW_STRDUP(xml_element->get_prefix().c_str());
  } else {
    // Element in a differnet namespace and the identity value in a different
    // namespace. Set the new namespace to the element and use the identity
    // prefix for value
    std::string attr_name = "xmlns";
    if (prefix && prefix[0]) {
      attr_name += ":";
      attr_name += prefix;
    }
    xml_element->set_attribute(attr_name.c_str(), ns, rw_xml_namespace_none);
  }

  std::string value_text;
  if (prefix &&  prefix[0]) {
    value_text = prefix;
    value_text += ":";
  }
  value_text += value->value_;

  xml_element->set_text_value(value_text.c_str());
}


/*****************************************************************************/
ParseLineResult::ParseLineResult(BaseCli& cli,
                                 const std::string& line_buffer,
                                 int options)
: cli_(cli),
  parse_tree_(std::move(cli.clone_current_mode_parse_node())),
  parse_node_(parse_tree_.get()),
  result_tree_(std::move(cli.clone_current_mode_parse_node())),
  result_node_(result_tree_.get()),
  result_mode_top_(NULL),
  line_buffer_(line_buffer),
  parse_options_(options),
  line_ends_with_space_(false),
  success_(false),
  error_(""),
  cli_print_hook_string_(NULL),
  cb_ (NULL)
{
  parse_line();
}

ParseLineResult::ParseLineResult(BaseCli& cli,
                                 const std::vector<std::string>& words,
                                 int options)
: cli_(cli),
  parse_tree_(std::move(cli.clone_current_mode_parse_node())),
  parse_node_(parse_tree_.get()),
  result_tree_(std::move(cli.clone_current_mode_parse_node())),
  result_node_(result_tree_.get()),
  result_mode_top_(NULL),
  line_words_(words),
  parse_options_(options),
  line_ends_with_space_(false),
  success_(false),
  error_(""),
  cli_print_hook_string_(NULL),
  cb_ (NULL)
{
  parse_line_words();
}

ParseLineResult::ParseLineResult(BaseCli& cli,
                                 ModeState& mode,
                                 const std::string& line_buffer,
                                 int options)
: cli_(cli),
  parse_tree_(std::move(mode.clone_top_parse_node())),
  parse_node_(parse_tree_.get()),
  result_tree_(std::move(mode.clone_top_parse_node())),
  result_node_(result_tree_.get()),
  result_mode_top_(NULL),
  line_buffer_(line_buffer),
  parse_options_(options),
  line_ends_with_space_(false),
  success_(false),
  error_(""),
  cli_print_hook_string_(NULL),
  cb_ (NULL)
{
  parse_line();
}

void ParseLineResult::parse_line()
{
  // Strip leading whitespace from line_buffer_
  ltrim(line_buffer_);

  // ATTN: TGS: Handle escapes!
  // Split the line_buffer_ into separate words
  if (line_buffer_.length() == 0) {
    line_ends_with_space_ = true;
    line_words_.clear();
  } else {
    line_ends_with_space_ = (*line_buffer_.rbegin() == ' ');
    if (RW_CLI_STRING_PARSE_STATUS_OK !=
        parse_input_line(line_buffer_, line_words_)) {
      success_ = false;
      return;
    }
  }

  parse_line_words();
}

void ParseLineResult::parse_line_words()
{
  bool override_cli_print_hook = true;
  parse_node_->app_data_clear();
  result_node_->app_data_clear();
  parse_node_->fill_children();
  parse_node_->next(this);

  // Set the CLI Print hook from the most recent ancestor if any
  for (ParseNode* pn = parse_node_; pn; pn = pn->parent_) {
    if (pn->is_cli_print_hook()) {
      cli_print_hook_string_ = pn->get_cli_print_hook_string();
      break;
    }
  }

  // Iterate through each word
  std::string word;
  std::vector<std::string>::size_type i;

  for (i = 0; i < line_words_.size(); ++i) {
    // Point to the next word and determine if this is the last word
    bool this_is_last_word = (i == line_words_.size() - 1);
    word = line_words_[i];

    if (parse_options_ & DASHES_MODE) {
      if (i == 0) {
        // Strip leading dashes on first word only.
        for (unsigned s=0; s<2 && word.size() && word[0] == '-'; ++s) {
          word = word.substr(1,word.size()-1);
        }
      } else {
        // Stop parsing when finding new leading dashes.
        if (word.size() && word[0] == '-') {
          // Truncate to fit.
          line_words_.resize(i);
          break;
        }
      }
    }

    // btrim(word);

    // Get the set of possible completions for this CLI word
    completions_.clear();
    parse_node_->get_completions(&completions_, word);

    // Check to see if processing of this word should stop
    // helpkey and tabkey actions do not autocomplete the last word
    if (this_is_last_word && !line_ends_with_space_ && !(parse_options_ & COMPLETE_LAST)) {
      // ATTN: This does not clone the last word - is that okay?
      break;
    }

    ParseNode* parse_found = nullptr;

    // Consume this CLI word only if it matches a single token
    if (completions_.size() == 1) {
      parse_found = completions_[0].node_;
    } else {
      /* A substring was entered, but had a space before the next word,
         or was the last word with a space - check for an exact match
         If a key word is a substring of another string, there could
         be multiple completions for the keyword. Check if there is
         an exact match for the word in the completions */
      for (ParseNode::compl_iter_t it = completions_.begin();
           // completions that include both partial matches and exact matches?
           it != completions_.end(); ) {
        if (!strcmp (it->node_->token_text_get(), word.c_str())) {
          if (!parse_found) {
            parse_found = it->node_;
          }
          ++it;
        } else {
          // Remove the non-exact keyword from the list of completions
          it = completions_.erase(it);
        }
      }
    }

    if (nullptr == parse_found) {
      error_ = "Did not find completion.";
      success_ = false;
      return;
    }

    std::string completed_value = parse_found->value_complete(word);
    if ((parse_options_ & NO_COMPLETE) && word != completed_value) {
      completions_.clear();
      success_ = false;
      error_ = "Could not auto-complete word.";
      return;
    }

    if (is_to_be_cooked(completed_value)) {
      completed_value = "\"" + completed_value + "\"";
    }

    line_words_[i] = completed_value;
    completed_buffer_ += line_words_[i] + " ";
    ;// ATTN: I feel the extra space shouldn't be added to complete commands on the last word

    adjust_subtrees(parse_found);

    // Keep track of the results.
    ParseNode::ptr_t result_found_ptr(parse_found->clone(result_node_));
    ParseNode* result_found = result_found_ptr.get();
    result_node_->adopt(std::move(result_found_ptr));

    parse_node_ = parse_found;
    result_node_ = result_found;

    // Parse app data?
    if (result_node_->flags_.is_set(ParseFlags::APP_DATA_LOOKUPS)) {
      bool keep_going = handle_app_data();
      if (!keep_going) {
        success_ = false;
        error_ = "Could not handle app data.";
        return;
      }
    }

    // Continue parsing as determined by the matched node
    parse_node_->next(this);
    RW_ASSERT(parse_node_);
    RW_ASSERT(result_node_);
    RW_ASSERT(parse_node_->is_clone(*result_node_));

    if (override_cli_print_hook &&
        parse_node_->is_cli_print_hook()) {
      cli_print_hook_string_ = parse_node_->get_cli_print_hook_string();
      if (parse_node_->flags_.is_set(ParseFlags::PRINT_HOOK_STICKY)) {
        // Print Hook is sticky, can't be overriden by other nodes
        override_cli_print_hook = false;
      }
    }

    if (parse_node_->get_callback()) {
      cb_ = parse_node_->get_callback();
    }
  }

  // If the line ends in space then get the completions for the last word
  if (line_ends_with_space_ && (parse_options_ != ENTERKEY_MODE)) {
    word = "";
    completions_.clear();
    parse_node_->get_completions(&completions_, "");
  }

  // ATTN: Control setting this via a parse flag.
  // Determine if this enters a mode.  In order to enter a mode, the
  // current parse target must be the top mode node.  If it is not,
  // that implies that the parsing state is targeting a mode's internal
  // leaf, or is not targeting a mode-related node at all.  Also, from
  // a purely practical perspective, if parse_node_ isn't the mode top
  // node, then it would be hard to find the mode top node in
  // result_tree_, and I don't want to write that code unless I
  // absolutely have to.
  ParseNode* parse_mode_top = parse_node_->mode_enter_top();
  if (parse_mode_top == parse_node_) {
    result_mode_top_ = result_node_;
  }

  // Return the allocated result structure
  last_word_ = word;
  success_ = true;
}

bool ParseLineResult::handle_app_data()
{
  // Iterate over all the tokens
  for (auto const& adpt: cli_.app_data_tokens_) {
    // Does the current result node have the token?
    if (result_node_->app_data_is_cached(adpt)) {
      // update first, only on first
      if (app_data_first_.size() <= adpt.app_data_index) {
        app_data_first_.resize(adpt.app_data_index+1);
      }
      if (!app_data_first_[adpt.app_data_index]) {
        app_data_first_[adpt.app_data_index] = result_node_;
      }

      // always update last
      if (app_data_last_.size() <= adpt.app_data_index) {
        app_data_last_.resize(adpt.app_data_index+1);
      }
      app_data_last_[adpt.app_data_index] = result_node_;

      // update the node itself
      if (result_node_->app_data_found_.size() <= adpt.app_data_index) {
        result_node_->app_data_found_.resize(adpt.app_data_index+1);
      }
      result_node_->app_data_found_[adpt.app_data_index] = result_node_;

      // make the callback, if any
      if (adpt.callback) {
        bool keep_going = (*adpt.callback)(adpt, this, result_node_);
        if (!keep_going) {
          return false;
        }
      }
    }
  }
  return true;
}

void ParseLineResult::adjust_subtrees(ParseNode* parse_found)
{
  // Adjust the parse tree's and result's tree's cursor node
  //  - In case of key keyword supression, the keyword node (not entered in the
  //    CLI line) should be inserted to parse tree and result tree
  //  - In the case of leaf-list, parent completions can be returned, here
  //    the tree cursor has to be moved up to the parent node

  if (parse_node_->is_hide_key_keyword() &&
      parse_found->is_key() && 
      !parse_found->flags_.is_set(ParseFlags::DEPRECATED)) {
    // The keyword for the key would have been suppressed
    // Add the keyword node to the parse tree and result tree
    parse_node_ = parse_found->parent_;
    parse_node_->flags_.set(ParseFlags::VISITED);
    parse_node_->value_complete("");

    ParseNode::ptr_t result_found_ptr(parse_node_->clone(result_node_));
    ParseNode* result_found = result_found_ptr.get();
    result_node_->adopt(std::move(result_found_ptr));
    result_node_ = result_found;
  }
  else if (parse_found->parent_ && parse_node_->parent_ &&
      parse_node_ != parse_found->parent_) {
    if (parse_node_ == parse_found) {
      // This can happen only for a list. Multiple lists in the same command
      RW_ASSERT(parse_node_->get_stmt_type() == RW_YANG_STMT_TYPE_LIST);

      // Move the result node and the parse_node to the parent
      result_node_ = result_node_->parent_;
      parse_found->clear_children();
    } else if (parse_node_->parent_ == parse_found->parent_) {
      // Siblings
      // This can happen in case of leaf-list where parent completions can be
      // returned. Pop the current result node and hide the current node from
      // the parent
      result_node_ = result_node_->parent_;
      parse_node_->parent_->hide(parse_node_);
    } else {
      // Check if the parse_found is sibling to one of the ancestors
      // Adjust the subtrees to that level
      ParseNode* rn = result_node_->parent_;
      ParseNode* pn = nullptr;
      for(pn = parse_node_->parent_; pn; 
          pn = pn->parent_, rn = rn->parent_) {
        if (pn == parse_found->parent_) {
          result_node_ = rn;
          break;
        }
      }
      RW_ASSERT(pn);
    }
  }
}

bool ParseLineResult::app_data_is_cached_first(
  const AppDataParseTokenBase& adpt,
  ParseNode** parse_node) const
{
  RW_ASSERT(parse_node);
  if (app_data_first_.size() > adpt.app_data_index) {
    *parse_node = app_data_first_[adpt.app_data_index];
    return true;
  }
  return false;
}

bool ParseLineResult::app_data_is_cached_last(
  const AppDataParseTokenBase& adpt,
  ParseNode** parse_node) const
{
  RW_ASSERT(parse_node);
  if (app_data_last_.size() > adpt.app_data_index) {
    *parse_node = app_data_last_[adpt.app_data_index];
    return true;
  }
  return false;
}

bool ParseLineResult::app_data_check_and_get_first(
  const AppDataParseTokenBase& adpt,
  ParseNode** parse_node,
  intptr_t* data) const
{
  if (app_data_first_.size() > adpt.app_data_index) {
    ParseNode* local_pn = app_data_first_[adpt.app_data_index];
    if (local_pn) {
      if (parse_node) {
        *parse_node = local_pn;
      }
      bool retval = local_pn->app_data_check_and_get(adpt, data);
      RW_ASSERT(retval); // Huh?  Changed during parse?  Don't do that!
      return true;
    }
  }
  return false;
}

bool ParseLineResult::app_data_check_and_get_last(
  const AppDataParseTokenBase& adpt,
  ParseNode** parse_node,
  intptr_t* data) const
{
  if (app_data_last_.size() > adpt.app_data_index) {
    ParseNode* local_pn = app_data_last_[adpt.app_data_index];
    if (local_pn) {
      if (parse_node) {
        *parse_node = local_pn;
      }
      bool retval = local_pn->app_data_check_and_get(adpt, data);
      RW_ASSERT(retval); // Huh?  Changed during parse?  Don't do that!
      return true;
    }
  }
  return false;
}


/*****************************************************************************/
/**
 * Create a base CLI object.
 */
BaseCli::BaseCli(YangModel& model)
: model_(model),
  root_mode_(NULL),
  current_mode_(NULL),
  prompt_(),
  root_parse_node_(new ParseNodeYang(*this, model.get_root_node(), NULL)),
  state_ (BaseCli::NONE),
  ms_adpt_(
    cliext_adpt_t::create_and_register(
      RW_YANG_CLI_MODULE, 
      RW_YANG_CLI_EXT_NEW_MODE,
      this)),
  ph_adpt_(
    cliext_adpt_t::create_and_register(
      RW_YANG_CLI_MODULE,
      RW_YANG_CLI_EXT_CLI_PRINT_HOOK,
      this)),
  skk_adpt_(
    cliext_adpt_t::create_and_register(
      RW_YANG_CLI_MODULE, 
      RW_YANG_CLI_EXT_SHOW_KEY_KW,
      this)),
  suppress_adpt_(
    cliext_adpt_t::create_and_register(
      RW_YANG_CLI_MODULE,
      RW_YANG_CLI_EXT_SUPPRESS_TOKEN,
      this))
{
  root_parse_node_->flags_.set_inherit(ParseFlags::APP_DATA_LOOKUPS);
}

/**
 * Destroy a base CLI object.
 */
BaseCli::~BaseCli()
{
  mode_stack_.clear();

  builtin_exit_.reset();
  builtin_end_.reset();
}

/**
 * Create tokens for the built-in commands that get inserted into all
 * non-root modes in the CLI.
 */
void BaseCli::add_builtins(void)
{
  ParseNodeBuiltin* exit_p = new ParseNodeBuiltin(*this, "exit", NULL);
  ParseNode::ptr_t exit_ptr(exit_p);
  builtin_exit_ = std::move(exit_ptr);
  static const char* exit_help = "Exit the current configuration mode";
  exit_p->help_short_set(exit_help);
  exit_p->help_full_set(exit_help);

  ParseNodeBuiltin* end_p = new ParseNodeBuiltin(*this, "end", NULL);
  ParseNode::ptr_t end_ptr(end_p);
  builtin_end_ = std::move(end_ptr);
  static const char* end_help = "Exit all configuration modes and return to exec mode";
  end_p->help_short_set(end_help);
  end_p->help_full_set(end_help);
}

/**
 * Add behavior nodes. The behavior nodes can appear at the root and at the start
 * of modes, and determine the behavior of visible children, completions and parsing
 *
 */

void BaseCli::add_behaviorals(void)
{
  ParseNodeBehavior* show_node = new ParseNodeBehavior (*this, "show", NULL);
  ParseNode::ptr_t tmp_ptr1(show_node);
  show_node_ = std::move(tmp_ptr1);

  static const char *show_help = "Show the configuration or operational data of the system";
  show_node->help_short_set (show_help);
  show_node->help_full_set (show_help);
  show_node->flags_.set_inherit (ParseFlags::HIDE_MODES);
  show_node->flags_.set_inherit (ParseFlags::HIDE_VALUES);
  show_node->flags_.set_inherit (ParseFlags::GET_GENERIC);
  show_node->set_callback (new CallbackBaseCli (this, &BaseCli::show_behavioral));

  ParseNodeBehavior* no_node = new ParseNodeBehavior (*this, "no", NULL);
  ParseNode::ptr_t tmp_ptr2(no_node);
  no_node_ = std::move(tmp_ptr2);

  static const char *no_node_help = "Remove configuration from the system";
  no_node->help_short_set (no_node_help);
  no_node->help_full_set (no_node_help);
  no_node->flags_.set_inherit (ParseFlags::CONFIG_ONLY);
  no_node->flags_.set_inherit (ParseFlags::HIDE_MODES);
  no_node->flags_.set_inherit (ParseFlags::HIDE_VALUES);
  no_node->set_callback (new CallbackBaseCli (this, &BaseCli::no_behavioral));

  ParseNodeBehavior* config_node = new ParseNodeBehavior (*this, "config", NULL);
  ParseNode::ptr_t tmp_ptr3(config_node);
  config_node_ = std::move(tmp_ptr3);

  static const char *config_node_help = "Enter the configuration mode";
  config_node->help_short_set (config_node_help);
  config_node->help_full_set (config_node_help);
  config_node->flags_.set_inherit (ParseFlags::CONFIG_ONLY);
  config_node->flags_.set (ParseFlags::IGNORE_MODE_ENTRY);
  config_node->set_is_mode(true);
  config_node->set_is_leafy(true);
  config_node->set_is_sentence(true);
  config_node->set_callback (new CallbackBaseCli (this, &BaseCli::config_behavioral));

  static const char *show_config_node_help = "Show running configuration";
  show_config_node_.reset(new ParseNodeBehavior(*this, "config", NULL));
  ParseNodeBehavior* show_config_node = static_cast<ParseNodeBehavior*>(
                                          show_config_node_.get());
  show_config_node->help_short_set(show_config_node_help);
  show_config_node->help_full_set(show_config_node_help);
  show_config_node->set_is_leafy(true);
  show_config_node->set_is_sentence(true);
  show_config_node->flags_.set(ParseFlags::GET_CONFIG);
  show_config_node->set_callback(new CallbackBaseCli(this, 
                                      &BaseCli::show_config));
  if (has_candidate_store_) {
    static const char *commit_node_help = "Commit the configuration";
    commit_node_.reset(new ParseNodeFunctional(*this, "commit", NULL));

    ParseNodeFunctional* commit_node = static_cast<ParseNodeFunctional*>(
        commit_node_.get());
    commit_node->help_short_set(commit_node_help);
    commit_node->help_full_set(commit_node_help);
    commit_node->set_is_leafy(true);
    commit_node->set_is_sentence(true);
    commit_node->set_callback (new CallbackBaseCli (this, 
          &BaseCli::commit_behavioral));

    static const char *discard_node_help = 
              "Discard the uncommitted configuration changes";
    discard_node_.reset(new ParseNodeFunctional(*this, "rollback", NULL));

    ParseNodeFunctional* discard_node = static_cast<ParseNodeFunctional*>(
                                          discard_node_.get());
    discard_node->help_short_set(discard_node_help);
    discard_node->help_full_set(discard_node_help);
    discard_node->set_is_leafy(true);
    discard_node->set_is_sentence(true);
    discard_node->set_callback (new CallbackBaseCli (this, 
                                          &BaseCli::discard_behavioral));

    static const char *show_cand_node_help = "Show candidate configuration";
    show_candidate_node_.reset(new ParseNodeBehavior(*this, 
                                        "candidate-config", NULL));
    ParseNodeBehavior* show_cand_node = static_cast<ParseNodeBehavior*>(
                                          show_candidate_node_.get());
    show_cand_node->help_short_set(show_cand_node_help);
    show_cand_node->help_full_set(show_cand_node_help);
    show_cand_node->set_is_leafy(true);
    show_cand_node->set_is_sentence(true);
    show_cand_node->flags_.set(ParseFlags::GET_CONFIG);
    show_cand_node->set_callback(new CallbackBaseCli(this, 
                                          &BaseCli::show_candidate_config));
  }
  // derived classes could enhance some behaviorals before they are added to a
  // parse tree.

  // Specifically, show gets enhanced with "config", "cli" etc
  enhance_behaviorals();
}

/**
 * Rebuild the prompt form the current mode.
 */
void BaseCli::setprompt(void)
{
  std::string new_prompt("rift");

  if (root_mode_ == current_mode_) {

    if ((BaseCli::STATE_CONFIG == state_)){
      new_prompt.append("(config)# ");
    }
    else {
      new_prompt.append("# ");
    }
  } else if (current_mode_->top_parse_node_->is_config()) {
    // ATTN: This test is lame - should really annotate this node, or verify its full path
    if ((current_mode_->top_parse_node_->token_text_get() == std::string("config"))){
      new_prompt.append("(config)# ");
    } else {
      new_prompt.append("(");
      new_prompt.append(current_mode_->top_parse_node_->get_prompt_string());
      new_prompt.append(")# ");
    }
  } else {
    new_prompt.append("(");
    new_prompt.append(current_mode_->top_parse_node_->get_prompt_string());
    new_prompt.append(")# ");

  }

  // Set the computed prmompt into the cli class
  // ATTN: Should prompt_ be a std::string?
  strncpy(prompt_, new_prompt.c_str(), sizeof(prompt_));
}

/**
 * Pop all modes, returning to the root node.  Pops the stack in
 * reverse order for safety's sake.  It is an error to do this if the
 * current mode is already the root mode.
 */
void BaseCli::mode_end()
{
  RW_ASSERT(mode_stack_.size() > 1);
  if (state_ == BaseCli::STATE_CONFIG) {
    bool ret = config_exit();
    if (!ret) {
      // cancel the mode exit
      return;
    }
  }

  while (mode_stack_.size() > 1) {
    mode_stack_.pop_back();
  }
  state_ = BaseCli::NONE;
  current_mode_ = root_mode_;

  // Reset flags
  current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::MODE_PATH);
  current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::CONFIG_ONLY);

  setprompt();
}

/**
 * Pop all modes, returning to the root node.  Pops the stack in
 * reverse order for safety's sake.  In contrast to mode_end(), this
 * function is allowed from root mode.  This function is intended for
 * use by code that just wants to return to the root mode, regardless
 * of the current mode.
 */
void BaseCli::mode_end_even_if_root()
{
  RW_ASSERT(mode_stack_.size() >= 1);
  
  if (mode_stack_.size() > 1) {
    mode_end();
    if (state_ != BaseCli::NONE) {
      // Mode end canceled
      return;
    }
  } else if (state_ == BaseCli::STATE_CONFIG) {
    // When exiting config mode invoke the config_exit method. This method may
    // be overridden and might want to cancel the mode exit
    bool ret = config_exit();
    if (!ret) {
      // cancel the mode exit
      return;
    }
  }

  state_ = BaseCli::NONE;
  current_mode_ = root_mode_;

  // Reset flags
  current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::MODE_PATH);
  current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::CONFIG_ONLY);

  setprompt();

}

/**
 * Pop the current mode.  Cannot pop the root mode.
 */
void BaseCli::mode_exit()
{
  if (mode_stack_.size() > 1) {
    // can exit from either config or the current mode, depending on the state
    // of the parse nodes
    if (state_ == BaseCli::STATE_CONFIG) {
      ModeState::reverse_stack_iter_t rit = mode_stack_.rbegin();
      rit++;

      if (rit->get()->top_parse_node_->flags_.is_clear (ParseFlags::CONFIG_ONLY)) {
        bool ret = config_exit();
        if (!ret) {
          // Config exit aborted by user
          return;
        }
        // Config was set on the current mode
        // There are better ways of doing this, but the cost seems to be a bit steep.
        state_ = BaseCli::NONE;

        // Reset flags
        current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::MODE_PATH);
        current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::CONFIG_ONLY);
      }
      else {
        mode_stack_.pop_back();
        current_mode_ = mode_stack_.back().get();
      }
    }
    else {
      mode_stack_.pop_back();
      current_mode_ = mode_stack_.back().get();
    }
  }
  else {

    // exit can be called from the root node, if the CLI is in config state
    // if in config state, just clear the state

    RW_ASSERT (state_ == BaseCli::STATE_CONFIG);
    bool ret = config_exit();
    if (!ret) {
      // Config exit aborted by user
      return;
    }
    state_ = BaseCli::NONE;

    // Reset flags
    current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::MODE_PATH);
    current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::CONFIG_ONLY);
  }

  setprompt();
}

/**
 * Pop to the specified mode (but don't pop the specified mode).  The
 * target mode may be the root node, but the current mode cannot be the
 * root mode.
 */
void BaseCli::mode_pop_to(ModeState* mode)
{
  RW_ASSERT(mode);
  if (mode == mode_stack_.back().get()) {
    // popping to current mode. This is allowed with automatic mode pops
    return;
  }
  RW_ASSERT(mode_stack_.size() > 1);
  while (mode_stack_.size() > 1) {
    if (mode == mode_stack_.back().get()) {
      break;
    }
    mode_stack_.pop_back();
  }
  current_mode_ = mode_stack_.back().get();
  RW_ASSERT(mode == current_mode_);
  setprompt();
}

/**
 * Enter a new mode, as defined by the input parse path.
 */
void BaseCli::mode_enter(ParseLineResult* r)
{
  ModeState::ptr_t new_mode(new ModeState(r));
  mode_enter_impl(std::move(new_mode));
}

/**

 * Enter a new mode, as defined by the input parse path.
 */
void BaseCli::mode_enter(ParseNode::ptr_t&& result_tree,
                         ParseNode* result_node)
{
  ModeState::ptr_t new_mode(new ModeState(std::move(result_tree), result_node));
  mode_enter_impl(std::move(new_mode));
}

/**
 * Enter a new mode, as defined by the input parse path.
 */
void BaseCli::mode_enter_impl(ModeState::ptr_t new_mode)
{
  RW_ASSERT(mode_stack_.size() < MAX_MODE_STACK_DEPTH);
  mode_stack_.push_back(std::move(new_mode));
  current_mode_ = mode_stack_.back().get();
  setprompt();
}

/**
 * Create the mode stack root.  MUST BE CALLED BY THE SUBCLASS after
 * construction and before trying to use the CLI!
 */
void BaseCli::mode_stack_root(ModeState::ptr_t new_root)
{
  RW_ASSERT(!root_mode_);
  RW_ASSERT(mode_stack_.empty());
  mode_stack_.push_back(std::move(new_root));
  current_mode_ = root_mode_ = mode_stack_.back().get();
}

bool BaseCli::config_exit()
{
  // Derived classes can customize the behavior on what to do when a config mode
  // is exited
  return true;
}

/**
 * Generate help for a partial line buffer.
 * ATTN: This return value is lame.
 */
bool BaseCli::generate_help(const std::string& line_buffer)
{
  // Create a parser class of the desired type and parse the command line
  ParseLineResult r(*this, line_buffer, ParseLineResult::NO_OPTIONS);
  if (!r.success_) {
    std::cout << "Error: Bad Input1" << std::endl;
    return false;
  }
  // If there are no completions, then the line should be complete
  if (r.completions_.size() < 1) {
    if (r.parse_tree_->is_sentence()) {
      std::cout << std::endl;
      // std::cout << "Short Help:" << std::endl;
      std::cout << "<ENTER> - complete the command" << std::endl;
    } else {
      std::cout << std::endl << "Error: Bad Input2" << std::endl;
    }
  }
  // If possible completions exist then display the help msg for them
  if (r.completions_.size() >= 1) {
    std::cout << std::endl;

    ParseNode* last_parent = r.completions_[0].node_->parent_;
    for (ParseNode::compl_size_t i = 0; i != r.completions_.size(); i++) {
      ParseCompletionEntry& entry = r.completions_[i];
      ParseNode* parse_node = entry.node_;

      if (parse_node->flags_.is_set(ParseFlags::DEPRECATED)) {
        // Ignore deprecated nodes for completion
        continue;
      }

      if ((r.last_word_.find(':') != std::string::npos) &&
          (!entry.prefix_required_)) {
        // Found a ':', display the node only if prefix is required
        continue;
      }

      if (parse_node->parent_ &&
          last_parent != parse_node->parent_ &&
          !parse_node->is_value()) {
        last_parent = parse_node->parent_;
        std::cout << "---- Completions for '" << last_parent->token_text_get()
                  << "' ----" << std::endl; 
      }

      if (parse_node->is_mode() && !parse_node->flags_.is_set (ParseFlags::HIDE_MODES)) {
        // Display a mode helpmsg for the completion
        std::string token_text = get_token_with_prefix(entry);
        std::cout << std::setw(15) << std::left << token_text << std::right
                  << " - Enter "
                  << token_text
                  << " mode: "
                  << parse_node->help_short_get()
                  << std::endl;
      } else if (parse_node->is_keyword()) {
        // Display a mode helpmsg for the completion
        std::string token_text = get_token_with_prefix(entry);
        std::cout << std::setw(15) << std::left << token_text << std::right
                  << " - KEYWORD: "
                  << parse_node->help_short_get()
                  << std::endl;
      } else {
        RW_ASSERT(parse_node->is_value());

        if (strcmp(parse_node->token_text_get(), "*") == 0) {
          std::cout << std::setw(15) << std::left
                  << parse_node->token_text_get() << std::right // ATTN: VALUE should be overridable?
                  << " - VALUE: "
                  << parse_node->help_short_get()
                  << std::endl;
        } else {
          // Display a mode helpmsg for the completion
          std::cout << std::setw(15) << std::left << "VALUE" << std::right // ATTN: VALUE should be overridable?
                  << " - type "
                  << parse_node->token_text_get()
                  << ": "
                  << parse_node->help_short_get()
                  << std::endl;
        }
      }
    }

    if (r.parse_tree_->is_sentence()) {
      std::cout << std::setw(15) << std::left << "<ENTER>" << std::right
                << " - complete the command" << std::endl;
    }
  }
  return true;
}

/**
 * Generates a vector of matches for the given command in the line_buffer.
 * For use with tab_complete and help generation. Returns the matches as a
 * vector unlike generate_help or tab_complete which prints to stdout.
 * @param[in] line_buffer  command for which completions to be returned
 * @return
 *  Returns a vector of matches
 */
std::vector<ParseMatch> BaseCli::generate_matches(const std::string& line_buffer)
{
  std::vector<ParseMatch> matches;
  // Create a parser class of the desired type and parse the command line
  ParseLineResult r(*this, line_buffer, ParseLineResult::NO_OPTIONS);
  if (!r.success_) {
    // Bad input
    return matches;
  }
  // If there are no completions, then the line should be complete
  if (r.completions_.size() < 1) {
    if (r.parse_tree_->is_sentence()) {
      matches.emplace_back("<ENTER>", "complete the command");
    } else {
      // bad input
    }
    return matches;
  }
  // If possible completions exist then display the help msg for them
  if (r.completions_.size() >= 1) {
    for (ParseNode::compl_size_t i = 0; i != r.completions_.size(); i++) {
      ParseCompletionEntry& entry = r.completions_[i];
      ParseNode* parse_node = entry.node_;

      if (parse_node->flags_.is_set(ParseFlags::DEPRECATED)) {
        // Ignore deprecated nodes for completion
        continue;
      }

      if ((r.last_word_.find(':') != std::string::npos) &&
          (!entry.prefix_required_)) {
        // Found a ':', display the node only if prefix is required
        continue;
      }

      if (parse_node->is_mode() && !parse_node->flags_.is_set (ParseFlags::HIDE_MODES)) {
        // Display a mode helpmsg for the completion
        std::stringstream disp;
        std::string token_text = get_token_with_prefix(entry);
        disp << "Enter " << token_text << " mode: " 
             << parse_node->help_short_get();
        matches.emplace_back(token_text, disp.str());
      } else if (parse_node->is_keyword()) {
        // Display a mode helpmsg for the completion
        std::stringstream disp;
        std::string token_text = get_token_with_prefix(entry);
        disp << "KEYWORD: " << parse_node->help_short_get();
        matches.emplace_back(token_text, disp.str());
      } else {
        RW_ASSERT(parse_node->is_value());
        std::stringstream disp;
        disp << "type " << parse_node->token_text_get() << ": " 
             << parse_node->help_short_get();
        matches.emplace_back("VALUE", disp.str());
      }
    }

    if (r.parse_tree_->is_sentence()) {
      matches.emplace_back("<ENTER>", "complete the command");
    }
  }

  return matches;
}

/**
 * Attempt TAB completion.
 * ATTN: This return value is lame.
 */
std::string BaseCli::tab_complete(const std::string& line_buffer)
{
  // Create a parser class of the desired type and parse the command line
  ParseLineResult r(*this, line_buffer, ParseLineResult::NO_OPTIONS);
  if (!r.success_) {
    std::cout << "Error: Bad Input1" << std::endl;
    return "";
  }

  // If there are no completions, then the line should be complete
  std::string linepath = r.completed_buffer_;
  if (r.completions_.size() < 1) {
    if (!r.parse_tree_->is_sentence()) {
      std::cout << std::endl << "Error: Bad Input2" << std::endl;
      return "";
    }
    if (linepath== line_buffer) {
      generate_help(line_buffer);
    }

    return linepath;
  }


  // If there is more than one completion, find the longest common prefix
  if (r.completions_.size() > 1) {
    int count = 0;
    if (!r.line_ends_with_space_) {
      std::string prefix;
      for (ParseCompletionEntry& entry: r.completions_) {
        if (entry.node_->flags_.is_set(ParseFlags::DEPRECATED)) {
          // Ignore deprecated nodes for completion
          continue;
        }
        if ((r.last_word_.find(':') != std::string::npos) &&
            (!entry.prefix_required_)) {
          // Found a ':', display the node only if prefix is required
          continue;
        }
        if (prefix.empty()) {
          prefix = get_token_with_prefix(entry);
        }
        std::string completion = get_token_with_prefix(entry);
        size_t j = 0;
        for(; j < std::min(prefix.size(), completion.size()); ++j) {
          if (prefix[j] != completion[j]) {
            break;
          }
        }
        prefix = prefix.substr(0, j);
        count++;
      }
      linepath = r.completed_buffer_ + prefix;
    } else {
      count = r.completions_.size();
    }

    if (count > 1) {
      generate_help(line_buffer);
    }
    if (count == 1 && !linepath.empty()) {
      linepath += " ";
    }
    return linepath;
  }

  // If there is only one completion, then perform the completion
  RW_ASSERT(r.completions_.size() == 1);
  ParseNode* parse_node = r.completions_[0].node_;
  std::string string;

  if (r.completions_[0].prefix_required_) {
    string = get_token_with_prefix(r.completions_[0]);
  } else {
    string = parse_node->value_complete(r.last_word_);
  }
  if (string.length() > 0) {
    linepath = r.completed_buffer_ + string + " ";
  } else {
    generate_help(line_buffer);
  }

  return linepath;
}

parse_result_t BaseCli::process_line_buffer(int argc, const char* const* argv, bool interactive)
{
  int count = argc;
  std::string line_buffer;
  line_buffer.reserve(128);

  while (count--) {
    line_buffer += " ";
    std::string token(*argv);

    if (escape_quotes(token) || is_to_be_cooked(token)) {
      line_buffer += "\"" + token + "\"";
    } else {
      line_buffer += token;
    }
    argv++;
  }
  return parse_line_buffer(line_buffer, interactive);
} 

/**
 * Parse a whole line.
 */
parse_result_t BaseCli::parse_line_buffer(const std::string& line_buffer,
                                          bool iterate)
{
  uint8_t levels;
  parse_result_t ret = PARSE_LINE_RESULT_INVALID_INPUT, curr_ret=PARSE_LINE_RESULT_INVALID_INPUT  ;
  bool result_set = false;
  ParseLineResult *r = nullptr;

  if (!iterate) {
    levels = 1;
  }
  else {
    // try parsing the line at all levels. "Magically" exit nodes if
    // a line would parse at a earlier node
    levels = mode_stack_.size();
  }

  // If the command line is empty then don't do anything
  std::string copy = line_buffer;
  btrim(copy);
  if (copy == "") {
    return PARSE_LINE_RESULT_SUCCESS;
  }

  ModeState::reverse_stack_iter_t rit = mode_stack_.rbegin();

  while (levels) {
    // Create a parser class of the desired type and parse the command line

    r = new ParseLineResult(*this, *(rit->get()), line_buffer, ParseLineResult::ENTERKEY_MODE);

    if (!r->success_) {
      curr_ret = PARSE_LINE_RESULT_INVALID_INPUT;
    } else if (r->completions_.size() < 1) {
      // If there are no completions, then the command is improper
      curr_ret = PARSE_LINE_RESULT_NO_COMPLETIONS;
    } else  if (r->completions_.size() > 1) {
      // If there is more than one completion, then the command is incomplete
      curr_ret = PARSE_LINE_RESULT_AMBIGUOUS;
    } else if (r->result_node_->is_built_in()) {
      curr_ret = PARSE_LINE_RESULT_SUCCESS;
      break;
    } else if (!r->parse_tree_->is_sentence()) {
      // If not a complete sentence, complain.
      curr_ret = PARSE_LINE_RESULT_INCOMPLETE;
    } else {
      // successful parse
      // If there is only one completion, then check if the sentence is complete
      curr_ret = PARSE_LINE_RESULT_SUCCESS;
      break;
    }
    rit++;
    if (!result_set) {
      // the first set error
      result_set = true;
      ret = curr_ret;
    }
    delete r;
    r = nullptr;
    levels --;
  }

  if (curr_ret != PARSE_LINE_RESULT_SUCCESS) {
    handle_parse_error (ret, line_buffer);
    delete r;
    return ret;
  } else if (r->parse_node_->is_rpc() || r->parse_node_->is_rpc_input()) {
    // This is RPC command, no auto mode exit would have been done
    // Get missing mandatory fields
    std::vector<std::string> missing_mandatory = r->parse_tree_->check_for_mandatory();
    if (!missing_mandatory.empty()) {
      std::stringstream missing_str;
      std::copy(missing_mandatory.begin(), missing_mandatory.end(),
                std::ostream_iterator<std::string>(missing_str, " "));

      ret = PARSE_LINE_RESULT_MISSING_MANDATORY;
      handle_parse_error(ret, missing_str.str());
      delete r;
      return ret;
    }
  }


  RW_ASSERT (nullptr != r);
  // pop to the mode in which the command was successful
  mode_pop_to(rit->get());

  if (r->result_node_->is_built_in()) {
    // Handle built-ins immediately.
    handle_command(r);
    delete r;
    return PARSE_LINE_RESULT_SUCCESS;
  }

  // the sentence should be complete in other cases
  RW_ASSERT(r->parse_tree_->is_sentence());


  if (nullptr != r->cb_) {
    r->cb_->execute(*r);
  } else if (r->result_mode_top_) {
    mode_enter(r);
  } else if (BaseCli::STATE_CONFIG == state_ ) {
    process_config (*r);
  } else {
    handle_command(r);
  }
  
  delete r;
  return PARSE_LINE_RESULT_SUCCESS;
}

void BaseCli::handle_parse_error(const parse_result_t result,
                                 const std::string line)
{
}
/**
 * Parse vector.  Returns the number of words eaten, if any.
 */
unsigned BaseCli::parse_argv(const std::vector<std::string>& argv)
{
  // Attempt to consume arguments until there is happiness.
  std::vector<std::string> local_argv;
  unsigned i = 0;
  for (; i < argv.size(); ++i ) {
    local_argv.push_back(argv[i]);

    ParseLineResult r(*this, local_argv, ParseLineResult::ARGV_MODE);
    if (!r.success_) {
      continue;
    }

    if (r.completions_.size() < 1) {
      return false;
    }

    if (r.completions_.size() > 1) {
      continue;
    }

    if (!r.parse_tree_->is_sentence()) {
      continue;
    }

    if (r.result_mode_top_) {
      mode_enter(&r);
      local_argv.clear();
      continue;
    }

    bool good = handle_command(&r);
    if (!good) {
      return i;
    }

    // Eat the arguments, and continue parsing.
    local_argv.clear();
  }

  return i;
}

/**
 * Parse argv vector as an explicit single command.  Mode entering is
 * not allowed!
 * @return true if successful, false otherwise.
 */
bool BaseCli::parse_argv_one(int argc, const char* const* argv)
{
  RW_ASSERT(0 == argc || argv);
  if (0 == argc) {
    return true;
  }

  // Attempt to consume arguments until there is happiness.
  std::vector<std::string> local_argv;
  for (int i = 0; i < argc; ++i ) {
    RW_ASSERT(argv[i]);
    local_argv.push_back(argv[i]);
  }

  ParseLineResult r(*this, local_argv, ParseLineResult::ARGV_MODE);
  if (!r.success_) {
    return false;
  }

  if (r.completions_.size() < 1) {
    return false;
  }

  if (r.completions_.size() > 1) {
    return false;
  }

  if (!r.parse_tree_->is_sentence()) {
    return false;
  }

  if (r.result_mode_top_) {
    return false;
  }

  return handle_command(&r);
}

/**
 * Invoke a series of command lines, specified in an argument vector.
 * At the end, any current modes will be exitted, leaving the CLI in
 * the root mode.
 * @return true if all commands were successful, otherwise false.
 */
bool BaseCli::parse_argv_set(int argc, const char* const* argv)
{
  RW_ASSERT(0 == argc || argv);
  if (0 == argc) {
    return true;
  }

  bool retval = true;
  for (int i = 0; i < argc; ++i ) {
    RW_ASSERT(argv[i]);
    bool ok = (PARSE_LINE_RESULT_SUCCESS == parse_line_buffer(argv[i], false));
    retval = retval && ok;
  }

  mode_end_even_if_root();
  return retval;
}

/**
 * Clone the current mode's parse node and set PARSE_TOP flag.
 */
ParseNode::ptr_t BaseCli::clone_current_mode_parse_node()
{
  RW_ASSERT(current_mode_);
  ParseNode::ptr_t clone = current_mode_->top_parse_node_->clone(current_mode_->top_parse_node_->parent_);
  clone->flags_.set(ParseFlags::PARSE_TOP);
  clone->flags_.set(ParseFlags::V_LIST_KEYS_CLONED);
  return clone;
}

/**
 * Add top level functional node
 */

void BaseCli::add_top_level_functional (ParseNodeFunctional *desc)
{
  ParseNodeFunctional::d_ptr_t temp (new ParseNodeFunctional (*desc, root_parse_node_.get()));
  top_funcs_.push_back (std::move (temp));
}

/**
 * function that handles the show behavioral
 * Derived classes should implement this functionality, as the base is unaware of
 * what is required.
 */

bool BaseCli::show_behavioral(const ParseLineResult& r)
{
  if (state_ == BaseCli::STATE_CONFIG) {
    show_config (r);
  } else {
    show_operational (r);
  }
  return true;
}

bool BaseCli::show_config (const ParseLineResult& r)
{
  UNUSED (r);
  return true;
}

bool BaseCli::show_candidate_config (const ParseLineResult& r)
{
  UNUSED (r);
  return true;
}

bool BaseCli::show_operational (const ParseLineResult& r)
{
  UNUSED (r);
  return true;
}

bool BaseCli::no_behavioral(const ParseLineResult& r)
{
  UNUSED (r);
  return true;
}

bool BaseCli::commit_behavioral(const ParseLineResult& r)
{
  // To be implemented in the derived class
  UNUSED (r);
  return true;
}

bool BaseCli::discard_behavioral(const ParseLineResult& r)
{
  // To be implemented in the derived class
  UNUSED (r);
  return true;
}

bool BaseCli::process_config(const ParseLineResult&r )
{
  UNUSED (r);
  RW_CRASH();
  return false;
}
/**
 * The config behavioral moves the CLI to the config mode
 */
bool BaseCli::config_behavioral (const ParseLineResult& r)
{

  RW_ASSERT (BaseCli::STATE_CONFIG != state_);

  state_ = BaseCli::STATE_CONFIG;
  // Reset flags if only mode paths were allowed
  current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::MODE_PATH);
  current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::CONFIG_ONLY);
  setprompt();
  return true;
}

AppDataParseTokenBase BaseCli::app_data_register(
  const char* ns,
  const char* ext,
  AppDataTokenBase::deleter_f deleter,
  AppDataCallback* callback)
{
  /*
   * Search for a duplicate.  ATTN: Linear search is dumb, but this is
   * assumed to be a rare operation....
   */
  {
    for (auto const& adpt: app_data_tokens_) {
      if (adpt.ns == ns && adpt.ext == ext) {
        return adpt;
      }
    }
  }

  // Not duplicate
  AppDataTokenBase ytoken;
  bool okay = model_.app_data_get_token(ns, ext, deleter, &ytoken);
  if (!okay) {
    /*
     * ATTN: Whoa, this is harsh, but hard to do anything better with
     * the current design.  And, anyway, 4096 tokens should be plenty
     * for anyone (just like 640K).
     */
    throw std::bad_alloc();
  }

  unsigned app_data_index = app_data_tokens_.size();
  AppDataParseTokenBase adpt(app_data_index, ns, ext, callback, ytoken);
  app_data_tokens_.push_back(adpt);
  RW_ASSERT((app_data_index+1) == app_data_tokens_.size());

  return adpt;
}

std::string BaseCli::get_token_with_prefix(const ParseCompletionEntry& entry)
{
  std::string token_text;
  if (entry.prefix_required_) {
    const char* prefix = entry.node_->get_prefix();
    if (prefix) {
      token_text = prefix;
      token_text += ":";
    }
    token_text += entry.node_->token_text_get();
  } else {
    token_text = entry.node_->token_text_get();
  }
  return token_text;
}

ParseNodeFunctional* BaseCli::find_descendent_deep(
                        const std::vector<std::string>& path) 
{
  bool is_top = true;
  ParseNodeFunctional* result = nullptr;
  for(auto& token: path) {
    if (is_top) {
      for(auto& desc: top_funcs_) {
        if (token == desc->token_text_get()) {
          result = desc.get();
          break;
        }
      }
      is_top = false;
    } else {
      result = result->find_descendent(token);
    }
    if (!result) {
      return result;
    }
  }
  return result;
}

bool BaseCli::suppress_namespace(const std::string& ns)
{
  // Find the YangModule for the given namespace and suppress all the top-level
  // nodes in that module.
  YangModule* module = model_.search_module_ns(ns.c_str());
  if (module == nullptr) {
    return false;
  }

  for(YangNodeIter it = module->node_begin(); it != module->node_end(); ++it) {
    YangNode& ynode = *it;
    ynode.app_data_set_and_keep_ownership(suppress_adpt_.get_yang_token(), "");
  }

  return true;
}

bool BaseCli::suppress_command_path(const std::string& path)
{
  // Spoof a cli command to get the last yang node
  ParseLineResult r(*this, path.c_str(), ParseLineResult::ENTERKEY_MODE);
  if (!r.success_) {
    // not found
    return false;
  }

  r.result_node_->set_suppressed();

  return true;
}


/*****************************************************************************/

/**
 * The CallbackBaseCli constructor
 */

CallbackBaseCli::CallbackBaseCli(BaseCli *data, BaseCliCallback cb)
    :cb_func_(cb), cb_data_ (data)
{}

/**
 * The execute method - call the callback function
 */
bool CallbackBaseCli::execute (const ParseLineResult &r)
{
  return (*cb_data_.*cb_func_)(r);
}

} /* namespace rw_yang */
