
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
 * @file yangcli.hpp
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @author Anil Gunturu (anil.gunturu@riftio.com.com)
 * @author Tom Seidenberg (tom.seidenberg@riftio.com)
 * @date 02/05/2014
 * @brief Top level include for RIFT CLI base
 */

// This file was previously in the following path
//   modules/core/utils/yangtools/include/riftware/yangcli.hpp
// For commit history refer the submodule modules/core/utils

#ifndef RW_YANGTOOLS_BASECLI_HPP_
#define RW_YANGTOOLS_BASECLI_HPP_

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#include <fstream>
#include <iostream>
#include <algorithm>
#include <string>
#include <list>
#include <vector>
#include <deque>
#include <memory>
#include <bitset>
#include <set>
#include <map>


#include "rwlib.h"
#include "rw_app_data.hpp"
#include "yangncx.hpp"
#include "util.hpp"
#include "rw_xml.h"

namespace rw_yang
{

class BaseCli;
class ModeState;
class ParseFlags;
class ParseLineResult;
class ParseNode;
class ParseNodeValue;
class ParseNodeYang;
struct ParseNodeSetSort;
class ParseNodeFunctional;
class ParseNodeBehavior;
class ParseNodeValueInternal;
class ParseNodeBuiltin;
class CliCallback;
template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>> class AppDataParseToken;
class AppDataParseTokenBase;
struct AppDataCallback;

typedef enum parse_result_t_ {
  PARSE_LINE_RESULT_SUCCESS = 0X280052014,
  PARSE_LINE_RESULT_INVALID_INPUT,
  PARSE_LINE_RESULT_NO_COMPLETIONS,
  PARSE_LINE_RESULT_AMBIGUOUS,
  PARSE_LINE_RESULT_INCOMPLETE,
  PARSE_LINE_RESULT_MISSING_MANDATORY
} parse_result_t;


/**
 * @defgroup YangCLI Yang based CLI Library
 * @{
 */

/**
 * Parser control flag object.  Used to indicate various interesting
 * things about the state of the parser.  Flags can be set in a node,
 * and optionally inherited by child nodes.  Inherited flags can be
 * inherited once, or through to all descendant nodes.
 */
class ParseFlags
{
 public:
  /// The flag constants
  enum flag_t
  {
    NONE = 0, ///< Zero-initialization value; not a valid flag.
    FLAG_base = 0x4CF100,
    CLI_ROOT, ///< The top most node in the parse tree
    PARSE_TOP, ///< top node of a parser context (is a ModeState.top_parse_node_).
    VISITED, ///< parser node has been visitted during the parse (not just populated)
    C_FILLED, ///< parser node's children have been filled in.
    V_FILLED, ///< parser node's visible children have been filled at least once.
    V_LIST_KEYS, ///< parser node is list and visible children is in keys mode.
    V_LIST_NONKEYS, ///< parser node is list and visible children is in non-keys.
    BEHAVIORALS_FILLED, ///< The behvioral nodes have been filled
    AUTO_MODE_CFG_CONT, ///< Containers with config are automatically new modes.
    AUTO_MODE_CFG_LISTK, ///< Lists with config are automatically new modes.
    NEXT_TO_SIBLING, ///< Allow popping to sibling nodes.  ATTN: Just a temporary hack
    HAS_VALUE, ///< Value has been set.
    V_LIST_KEYS_CLONED, ///< Keys of this list were made visible, and this flag can be cloned
    HIDE_MODES, ///< Hide modes: For show/no behavior nodes, this is set [show/no]
    HIDE_VALUES, ///< Hide value nodes of leafs - for [show/no]
    CONFIG_ONLY, ///< Limit completions to nodes that are config NOT set to false [config/no]
    MODE_PATH, ///< Limit completions to nodes that will lead to a mode.
    IGNORE_MODE_ENTRY, ///< ignore a mode entry if a node with this flag tries to do a node entry [config]
    APP_DATA_LOOKUPS, ///< perform app data lookups in the yang model
    DEPRECATED, ///< Use of this node is deprecated
    GET_GENERIC, ///< Behavioral node used for Getting config or operational data
    GET_CONFIG, ///< Behavioral node used in Get-Config operation
    PRINT_HOOK_STICKY, ///< Print Hook at this node cannot be overriden by descendants
    FLAG_end,

    FIRST = FLAG_base+1,
    LAST = FLAG_end-1,
  };

  /// The number of valid flag constants
  static const size_t COUNT = FLAG_end - FLAG_base - 1;

public:
  typedef std::bitset<COUNT> bitset_t;
  typedef size_t index_t;

public:
  /**
   * Check whether a flag value is good and not 0.
   *
   * @return
   *  - true: The flag value is good, and safe to index bitset_t with.
   *  - false: The flag value is not good.
   */
  static bool is_good(
    /// [in] The flag value to check.
    flag_t flag
  );

  /**
   * Check whether a flag value is good, or not good but
   * zero-initialized.
   *
   * @return
   *  - true: The flag value is good or zero-initialized.
   *  - false: The flag value is not good or zero-initialized.
   */
  static bool is_good_or_none(
    /// [in] The flag value to check.
    flag_t flag
  );

  /**
   * Convert a flag value to an index for use with bitset_t.
   *
   * @return The index.
   */
  static size_t to_index(
    /// [in] The flag value to convert.
    flag_t flag
  );

  /**
   * Convert a index to a flag value.
   *
   * @return The flag value.
   */
  static flag_t from_index(
    /// [in] The index to convert.
    size_t index
  );

 public:
  /**
   * Check if a parser control flag is set.
   *
   * @return
   *  - true: the flag is set.
   *  - false: the flag is not set.
   */
  bool is_set(
    /// [in] The flag to check.
    flag_t flag
  ) const;

  /**
   * Check if a parser control flag is clear.
   *
   * @return
   *  - true: the flag is clear.
   *  - false: the flag is not clear.
   */
  bool is_clear(
    /// [in] The flag to check.
    flag_t flag
  ) const;

  /**
   * Clear a parser flag.  Has no effect if the flag is already clear.
   * Has no effect on flag inheritance.
   */
  void clear(
    /// [in] The flag to clear.
    flag_t flag
  );

  /**
   * Set a parser flag.  Has no effect if the flag is already set.  Has
   * no effect on flag inheritance.
   */
  void set(
    /// [in] The flag to set.
    flag_t flag
  );

  /**
   * Set a parser flag to a particular value.  Has no effect if the
   * flag already has that value.  Has no effect on flag inheritance.
   */
  void set(
    /// [in] The flag to set.
    flag_t flag,

    /// [in] The new value of the flag.
    bool new_value
  );

  /**
   * Clear a flag and stop inheriting it.  Inheritance state will
   * change even if flag is already clear.  Has no effect on the
   * inherit-once state.
   */
  void clear_inherit(
    /// [in] The flag to clear.
    flag_t flag
  );

  /**
   * Set a flag and cause it to be inherited.  Inheritance state will
   * change even if flag is already set.  Has no effect on the
   * inherit-once state.
   */
  void set_inherit(
    /// [in] The flag to set.
    flag_t flag
  );

  /**
   * Set a parser flag and inheritance state to a particular value.  If
   * cleared, inheritance will also be cleared.  If set, inheritance
   * will also be set.  Has no effect on the inherit-once state.
   */
  void set_inherit(
    /// [in] The flag to set.
    flag_t flag,

    /// [in] The new value of the flag.
    bool new_value
  );

  /**
   * Clear a flag and stop inheriting it once.  Inherit-once state will
   * change even if flag is already clear.  Has no effect on unlimited
   * inheritance state.
   */
  void clear_inherit_once(
    /// [in] The flag to clear.
    flag_t flag
  );

  /**
   * Set a flag and cause it to be inherited.  Inherit-once state will
   * change even if flag is already set.  Has no effect on unlimited
   * inheritance state.
   */
  void set_inherit_once(
    /// [in] The flag to set.
    flag_t flag
  );

  /**
   * Set a parser flag and inherit-once state to a particular value.
   * If cleared, inherit-once will also be cleared.  If set,
   * inherit-once will also be set.  Has no effect on unlimited
   * inheritance state.
   */
  void set_inherit_once(
    /// [in] The flag to set.
    flag_t flag,

    /// [in] The new value of the flag.
    bool new_value
  );

  /**
   * Copy the flags for a child parse node, obeying flag inheritance,
   * and initializing the child's inheritance list according to the
   * inherit-once state to prevent and further inheritance for
   * inherit-once flags.
   *
   * @return The child's flags.
   */
  ParseFlags copy_for_child() const;

  /**
   * The flags belong to a child node that was cloned into a cloned
   * parent node.  Merge the new parent's inherited flags into these
   * flags, for use in a parallel parse node tree.  Flags that don't
   * make sense in the context of a cloned parallel tree are cleared
   * automatically.
   */
  void clone_and_inherit(
    /// [in] The clone parent's flags.
    const ParseFlags& new_parent_flags
  );

public:
  /// The set flags.
  bitset_t set_;

  /// The flags that should be inherited by all descending nodes.
  bitset_t inherit_;

  /// The flags that should be inherited only by immediate child nodes,
  /// and not those nodes' descendants.
  bitset_t inherit_once_;
};

/**
 * For use with generate_matches() to return a list of possible completions
 */
class ParseMatch {
 public:
  /**
   * Default constructor. Not used.
   */
  ParseMatch()
  {}

  /**
   * Initializes the ParseMatch with a match string and description.
   */
  ParseMatch(const std::string& match, const std::string& descr)
    : match_(match),
      description_(descr)
  {
  }

  ~ParseMatch()
  {}

 public:
  /** Match string */
  std::string match_;

  /** Description for the match string */
  std::string description_;

  // More fields shall be added for grouping etc...
};


/**
 * Callback object for application data.
 */
class AppDataCallback {
 public:
  /// Trivial constructor
  AppDataCallback()
  {}

  /// Trivial destructor
  virtual ~AppDataCallback()
  {}

  /**
   * Invoke the callback.  The callbacks are made in ascending order of
   * parse_slot_index, as result nodes are added to the parse result
   * tree.  The callback is made before the call to next().  The
   * callback may modify the parse line results, which is allowed to
   * change the behavior of the parser.  If the callback wishes to
   * terminate the parse with failure, it should return false.
   *
   * @return
   *  - true: Keep parsing
   *  - false: Stop parsing, with an error.
   *  - ATTN: There should be a better return value.
   */
  virtual bool operator()(
    /// [in] The application data token information.
    const AppDataParseTokenBase& adct,

    /// [in/out] The parse line results.  May be modified by the callback.
    ParseLineResult* r,

    /// [in/out] The parse node.  May be modified by the callback.
    ParseNode* result_node
  ) = 0;
};


/**
 * Application-data registration tracker.
 */
class AppDataParseTokenBase
{
 public:
  /// Construct a token.
  AppDataParseTokenBase(
    /// The parser assigned lookup index.
    unsigned i_app_data_index,

    /**
     * [in] The namespace of the extension.  Doesn't have to be a
     * namespace of a loaded yang module, in which case only extensions
     * that are explicitly set in the yang node would be found.
     */
    const char* i_ns,

    /// [in] The name of the extension.
    const char* i_ext,

    /**
     * [in] The parser callback to make, during parse, each time the
     * extension is found on a result-tree parse node that has been
     * visited.  This callback should be used ONLY to modify the parser
     * behaviour, such as by setting parse node flags.  DO NOT USE THIS
     * FOR APPLICATION CALLBACKS!  The parse is not complete when this
     * gets called, so you can't know if the command is complete or
     * even valid.
     */
    AppDataCallback* i_callback,

    /**
     * The yang model application data token associated with the cli
     * token.  This is used to lookup the extension in the yang model
     * during parse.  This was created by the yang model.
     */
    AppDataTokenBase i_ytoken
  );

  /// Copyable, use default copy constructor.
  AppDataParseTokenBase(const AppDataParseTokenBase&) = default;

  /// Use default constructor.
  AppDataParseTokenBase() = default;

 public:
  /**
   * Check a parse node for this application data.  Presumably, the
   * parse node is obtained from a parse line results.
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  bool parse_node_is_cached(
    /// [in] The parse node to check
    const ParseNode* parse_node
  ) const;

  /**
   * Check a parse line result for this application data.  If any parse
   * node in the result had cached the application data, return the
   * first parse node that did.
   *
   * @return
   *  - true: The value was cached by at least one parse node.
   *  - false: The value was not cached by any parse node.
   */
  bool parse_result_is_cached_first(
    /// [in] The parse line results to check.
    /// Cannot be nullptr.
    const ParseLineResult* plr,

    /// [out] The first parse node that had cached data.
    /// Cannot be nullptr.
    ParseNode** parse_node
  ) const;

  /**
   * Check a parse line result for this application data.  If any parse
   * node in the result had cached the application data, return the
   * last parse node that did.
   *
   * @return
   *  - true: The value was cached by at least one parse node.
   *  - false: The value was not cached by any parse node.
   */
  bool parse_result_is_cached_last(
    /// [in] The parse line results to check.
    /// Cannot be nullptr.
    const ParseLineResult* plr,

    /// [out] The last parse node that had cached data.
    /// Cannot be nullptr.
    ParseNode** parse_node
  ) const;

 public:
  /**
   * The parser assigned lookup index.  This value is used to index
   * app data vectors within the cli, parse node, and parse node
   * results.
   */
  const unsigned app_data_index;

  /// The namespace of the application data's extension.
  std::string ns;

  /// The name of the application data extension.
  std::string ext;

  /**
   * Parser callback.  May be nullptr.  This was assigned by the
   * application.
   */
  AppDataCallback* callback;

  /**
   * The yang model application data token associated with the cli
   * token.  This is used to lookup the extension in the yang model
   * during parse.  This was assigned by the yang model.
   */
  AppDataTokenBase ytoken;
};


/**
 * Application-data registration tracker.  These objects are used to
 * store the yang model application data tokens, which are then used by
 * the parser to check each visited node for application data.  Found
 * application data nodes are retained in the parse results.  The
 * command handlers can then use the application data to modify
 * behavior, in a model-driven way.
 *
 * How to use this class:
 *  - In your derived CLI object, create one of these objects for each
 *    application data extension you need to support.  These get
 *    created at CLI instantiation time.  (ATTN: Need to pass them to
 *    child CLIs)
 *  - For each one of your application data extensions, implement code
 *    to handle the extension, either in handle_command(), or as a
 *    AppDataCallback.
 *     - If handle_command(), your extension will only work on complete
 *       commands.  When the command completes, your code should
 *       inspect the parse line results, to determine which parse nodes
 *       had the extensions (if any).  Use this mode if you need to
 *       modify the handling of complete commands.
 *     - If using AppDataCallback, then for each visitted parse node, the
 *       callback will be made if the parse node has the extension.
 *       The callbak will have complete access to the parse line
 *       results, and may modify the state of the parser as necessary.
 *       Use this mode if you need to modify the behavior of the parser
 *       at parse time.
 *     - Both modes may be used for a single extension.
 */
template <class data_type, class deleter_type>
class AppDataParseToken
: public AppDataParseTokenBase
{
 public:
  typedef AppDataToken<data_type,deleter_type> yang_token_t;

 public:
  /// Copyable, use default copy constructor.
  AppDataParseToken(const AppDataParseToken&) = default;

  // Don't allow default constructor.
  AppDataParseToken() = delete;

  /**
   * Create a parsing token.
   */
  static AppDataParseToken create_and_register(
    /**
     * [in] The namespace of the extension.  Doesn't have to be a
     * namespace of a loaded yang module, in which case only extensions
     * that are explicitly set in the yang node would be found.
     */
    const char* ns,

    /// [in] The name of the extension.
    const char* ext,

    /// [in] The parser to register with.
    BaseCli* parser,

    /**
     * [in] The parser callback to make, during parse, each time the
     * extension is found on a result-tree parse node that has been
     * visited.  This callback should be used ONLY to modify the parser
     * behaviour, such as by setting parse node flags.  DO NOT USE THIS
     * FOR APPLICATION CALLBACKS!  The parse is not complete when this
     * gets called, so you can't know if the command is complete or
     * even valid.
     */
    AppDataCallback* callback = nullptr
  );

  /**
   * Check a parse node for this application data.  Presumably, the
   * parse node is obtained from a parse line results.  If the parse
   * node has the application data, return it.
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  bool parse_node_check_and_get(
    /// [in] The parse node to check
    ParseNode* parse_node,

    /// [out] The data value, if cached.
    data_type* data
  ) const;

  /**
   * Check a parse line result for the first node that had cached this
   * application data.  If a parse node had the application data,
   * return the data and (optionally) the node.
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  bool parse_result_check_and_get_first(
    /// [in] The parse line results to check.  Cannot be nullptr.
    const ParseLineResult* plr,

    /// [out] The first parse node that had cached data, if any.  May
    /// be nullptr if the node is not wanted.
    ParseNode** parse_node,

    /// [out] The data value, if cached.  Cannot be nullptr.
    data_type* data
  ) const;

  /**
   * Check a parse line result for the last node that had cached this
   * application data.  If a parse node had the application data,
   * return the data and (optionally) the node.
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  bool parse_result_check_and_get_last(
    /// [in] The parse line results to check.  Cannot be nullptr.
    const ParseLineResult* plr,

    /// [out] The last parse node that had cached data, if any.  May
    /// be nullptr if the node is not wanted.
    ParseNode** parse_node,

    /// [out] The data value, if cached.  Cannot be nullptr.
    data_type* data
  ) const;

  /**
   * Get a pointer to the fully-typed yang application data token.
   *
   * @return A pointer to the yang token.
   */
  const yang_token_t* get_yang_token() const;
};


/**
 * Helper class for a set of nodes, to provide sort function.
 */
struct ParseNodeSetSort
{
  bool operator() (const ParseNode* a, const ParseNode* b);
};

/**
 * Class that holds the parse completion information.
 */
class ParseCompletionEntry
{
public:
  /**
   * Constructs the Parse completion entry
   */
  ParseCompletionEntry(
      /// [in] Node that was parse completed
      ParseNode* node, 
      
      /// [in] Completion requires Yang model prefix
      bool prefix_required
  )
    : node_(node),
      prefix_required_(prefix_required)
  {
  }

  ParseCompletionEntry(
      /// [in] Node that was parse completed
      ParseNode* node
  )
    : ParseCompletionEntry(node, false)
  {
  }

public:

  /// Parse Node that is part of the completion
  ParseNode*  node_ = nullptr;

  /// Indicates that Yang Model prefix is required. This will be set to True if
  /// there was a conflict of token names among the list of current completions
  bool        prefix_required_ = false;
};


/**
 * Parser Node.  A parser node exists to assist the CLI parser in
 * following the schema and keeping track of values.  Parsing can occur
 * in several contexts: FILE or interactive input of a complete line,
 * an explicit call to parse an arbitrary string, help or
 * tab-completion callbacks.
 *
 * Parser nodes exist in several other data structures:
 *  - Statically as part of the CLI base support, for some internal
 *    commands.
 *  - Saved parser state captured in CLI mode states.  These parser
 *    nodes allow the parser to continue where it left off after
 *    exiting a mode.
 *  - Parser tree nodes, which are used during line buffer parsing to
 *    determine what keywords are allowed and/or are viable
 *    completions.
 *  - Parser path nodes, which keep track of the specific sequence of
 *    nodes traversed so far in a line buffer parse.
 *
 * Parser nodes form a rooted tree.  The root may or may not be
 * identified as a specific token.  Entire trees can be deleted by
 * deleting the root.  Subtrees can be deleted by deleting the unwanted
 * child from the children_ list of the parent node.  Cleanup of
 * descendant nodes is automatic.
 *
 * Nodes cannot be members of more than one tree, nor can they be
 * copied directly.  However, nodes can be cloned into another tree.
 * The parser commonly maintains 2 parallel trees during a parse - a
 * fat tree containing all the nodes that are valid nex nodes from the
 * current node, and a result tree, which contains only those nodes
 * that were actually visited (that appeared in the input).
 *
 * ATTN: Support for pipelines, job control
 * ATTN: would be nice to insert "help" token into parse tree that redirects to current root:
 *       parse_root -> child -> grandchild
 *       parse_root -> help -> child -> grandchild
 * ATTN: what about i18n of help text?
 */
class ParseNode
{
 public:
  typedef std::unique_ptr<ParseNode> ptr_t;

  typedef std::vector<ParseCompletionEntry> completions_t;
  typedef completions_t::iterator compl_iter_t;
  typedef completions_t::size_type compl_size_t;

  typedef std::set<ParseNode*,ParseNodeSetSort> visible_t;
  typedef visible_t::iterator vis_iter_t;
  typedef visible_t::const_iterator vis_citer_t;

  typedef std::list<ptr_t> children_t;
  typedef children_t::iterator child_iter_t;
  typedef children_t::const_iterator child_citer_t;


 public:
  /**
   * Create a root parse node that is not associated with any model node.
   */
  explicit ParseNode(
    /// [in] The owning parser.
    BaseCli& cli
  );

  /**
   * Create a parse node, possibly descended from a specific parent node.
   */
  ParseNode(
    /// [in] The owning parser.
    BaseCli& cli,

    /// [in] The parent node.  May be nullptr.
    ParseNode* parent
  );

  /**
   * Copy-construct a parse node, giving the new node a new parent.
   *
   * This constructor provides a strong exception guarantee - if the
   * constructor throws, then nothing is leaked and no nodes are left in
   * a bad state.
   */
  ParseNode(
    /// [in] The parse node being cloned.
    const ParseNode& other,

    /// [in] The parent node.  May be nullptr.
    ParseNode* parent
  );

  /// Trivial destructor.
  virtual ~ParseNode();

  // no copying
  ParseNode(const ParseNode& other) = delete;
  ParseNode& operator=(const ParseNode& other) = delete;

  /**
   * Clone a parse node into a parallel parse node tree.  All concrete
   * subclasses must implement this method in order to avoid object
   * slicing.  The children are not cloned, but the data in the node
   * may be.  The parser flags will be modified appropriately for the
   * other tree.
   *
   * @return The cloned node; owned by the caller until the caller
   *   adopts the node into another tree.
   */
  virtual ptr_t clone(
    /// [in] The parent node in the other tree.  May be nullptr.  If
    /// not nullptr, new_parent and this node's parent must be clones!
    ParseNode* new_parent
  ) const = 0;

 public:
  /**
   * Adopt a parse node.  It is currently owned by the caller.  The
   * child's parent must already be set to this.  This API implements
   * move semantics.
   */
  virtual void adopt(
    /// [in/out] The node to adopt.  Move semantics!  Pointer will be
    /// nulled.
    ptr_t&& new_child
  );

  /**
   * Check if this node is a clone of another node.  Clone detection
   * compares a subclass-defined data structure, which subclasses
   * should ensure is unique across all possible instances.  To detect
   * a clone, build the subclass-defined comparison structure and pass
   * it to the other node's is_clone_visit() method.  That method will
   * also build a clone comparison structure and then compare the two
   * structures.  YOU CANNOT COMPARE INDIRECT VALUES VIA POINTERS.
   *
   * A node is NOT considered a clone of itself!
   *
   * @return
   *  - true: The nodes are clones.
   *  - false: The nodes are not clones.
   */
  virtual bool is_clone(
    /// [in] The node to compare to.
    const ParseNode& other
  ) const = 0;

  /**
   * Check if this node is a clone of another node, by comparing the
   * other node's comparison data.
   *
   * @return
   *  - true: The nodes are clones.
   *  - false: The nodes are not clones.
   */
  virtual bool is_clone_visit(
    /// [in] The comparison data buffer.
    const void* ocv,

    /// [in] The size of the comparison data buffer.
    size_t size
  ) const = 0;

  /**
   * Given a root and target descendant node in one parse tree, find
   * the corresponding descendent node in the parse tree rooted in this
   * node.  This node must be a clone of the target root node, and the
   * target descendent node must be located beneath the target root
   * node.  Otherwise the results are undefined.
   *
   * @return This parse tree's node that corresponds to other_node.
   */
  virtual ParseNode* find_clone(
    /// [in] The root of the other parse tree.  This node must be a
    /// clone.
    const ParseNode& other_root,

    /// [in] The target node in the other tree.  The return value, if
    /// found, will be a clone of other_node.
    const ParseNode& other_node
  );

  virtual bool is_leafy() const;
  virtual bool is_key() const;
  virtual bool is_config() const;
  virtual bool is_rpc() const;
  virtual bool is_rpc_input() const;
  virtual bool is_notification() const;
  virtual bool is_value() const;
  virtual bool has_keys() const;
  virtual bool is_mode() const;
  virtual bool is_cli_print_hook() const;
  virtual const char* get_cli_print_hook_string();
  virtual bool is_built_in() const;
  virtual bool is_presence() const;
  virtual bool is_keyword() const;
  virtual bool is_sentence() const;
  virtual bool is_mandatory() const;

  /**
   * Check if the Yang node is supressed for CLI use
   */
  virtual bool is_suppressed() const;

  /**
   * Checks for missing mandatory fields. Returns true if all mandatory fields
   * are present. Else returns false and also returns the missing token(s)
   */
  virtual std::vector<std::string> check_for_mandatory() const;

  /**
   * Should mode entry be ignored for a node?  Some node behave like
   * modes, but just change behavior - like config
   */
  virtual bool ignore_mode_entry();

  /**
   * For the flags passed in, check if the node is "suitable".  The
   * flags are usually owned by a parent node, that needs to figure out
   * which children qualify
   */
  virtual bool is_suitable(const ParseFlags& flags) const;

  virtual bool is_mode_path() const;
  virtual bool is_functional() const;
  virtual const char *get_ns();
  virtual const char *get_prefix();
  virtual rw_yang_stmt_type_t get_stmt_type() const;
  virtual void set_mode (const char *display);
  virtual void set_cli_print_hook (const char *api);

  /**
   * Mark the underlying YangNode as suppressed from CLI use
   */
  virtual void set_suppressed();

  virtual ParseNode* mode_enter_top();

  virtual const char* token_text_get() const = 0;
  virtual const char* help_short_get() const = 0;
  virtual const char* help_full_get() const = 0;
  // ATTN: verbose help, that includes reference, children, and some decorations?
  // ATTN: full help: description, range, restriction string, default value

  /**
   * When this node is the top node in a CLI mode, and the CLI is in
   * interactive mode, get a string that represents this node. If the node
   * is a container, the prompt should be the name. For lists, it is a
   * keyword representing the list name and the values that make up the key
   * of the list element.
   *
   * Overriden only for ParseNodeYang, all types return token_text_get() in
   * the base class
   *
   * @return The string to display when this node is the top node in a mode
   */
  virtual const char* get_prompt_string();

  /**
   * Get a reference to the node's previously parsed value.  If a
   * keyword, it may or may not be completed, depending on the parser's
   * state at the time the node was parsed.  May be the empty string,
   * which is not necessarily an indication that the node has node
   * previously been parsed.
   */
  virtual const std::string& value_get() const;

  /**
   * Set the parsed value of the node.  Assumes that the value matches;
   * no checks are performed.
   */
  virtual void value_set(const std::string& value);

  /**
   * Determine if the value is a match to the token text.
   *
   * In case check_prefix is set to true, the value will be checked against the
   * Yang model prefix too. This is required in case there is conflict among the
   * completion tokens.
   */
  virtual bool value_is_match(
                  /// [in] value to be checked for a token match
                  const std::string& value,

                  /// [in] indicates that model prefix to be checked
                  bool check_prefix
               ) const;

  /**
   * Try to complete a word based on the current parse node.  Assumes
   * that the word matches the token text; no checks are performed.
   */
  virtual const std::string& value_complete(const std::string& value);

  /**
   * Calculate the next parse node - the node that defines the allowed
   * completions for the next token of input.
   *
   * If the function changes plr->parse_node, it must also change
   * plr->result_node to the corresponding clone.
   */
  virtual void next(
    /// [in/out] The current parse line results.  May be modified upon
    /// return to point to a new target parse node.
    ParseLineResult* plr
  ) = 0;

  /**
   * Hides the given node from the next subsequent completions. Also
   * makes other choice nodes in the same level as the provided node 
   * invisible
   */
  virtual void hide(
    /// [in] The node to hide.
    ParseNode* hide);

  /**
   * Calculate the next parse node - the node that defines the allowed
   * completions for the next token of input.  As the first step in the
   * calculation, hide the specified child of the parse_node.  The
   * specified node was previously matched by the parser, and it has no
   * more visible tokens left within the node or its descendants.  Make
   * the child invisible, and then determine if this node should also
   * be hidden.
   *
   * Also make invisible any other nodes that become unavailable when the
   * child of a choice node is selected. This is done by calling a function
   * that evaluates conflicts based on the type of the node.
   *
   * If the function changes plr->parse_node, it must also change
   * plr->result_node to the corresponding clone.
   */
  virtual void next_hide(
    /// [in] The node to hide.
    ParseNode* hide,

    /// [in/out] The current parse line results.  May be modified upon
    /// return to point to a new target parse node.
    ParseLineResult* plr
  );

  /**
   * Calculate the next parse node - the node that defines the allowed
   * completions for the next token of input.  As the first step in the
   * calculation, hide all child nodes of this node.  This happens when
   * the child is a simple value (out of multiple possible such
   * values).  Make all the children invisible, and then determine if
   * this node should also be hidden.
   *
   * If the function changes plr->parse_node, it must also change
   * plr->result_node to the corresponding clone.
   */
  virtual void next_hide_all(
    /// [in/out] The current parse line results
    ParseLineResult* plr
   );

  /**
   * Clear list of child nodes.
   */
  virtual void clear_children();

  virtual void fill_children();

  /**
   * Rebuild the list of visible children from the list of children
   * nodes.
   */
  virtual void fill_visible_all();

  virtual void fill_rpc_input();
  virtual void add_builtin_behavorial_to_visible();

  /**
   * Set a specific child node as visible
   */
  virtual void add_visible(
                  /// [in] The child node to be added to visible
                  ParseNode* child);


  /**
   * Build a list of child nodes that can complete a value, based on the
   * current set of visible child nodes, and add them to the given list.
   * The completions are based on pointers to the current children - THE
   * CALLER IS RESPONSIBLE FOR MAKING SURE THIS NODE IS NOT MODIFIED
   * while making use of the completions.
   */
  virtual void get_completions(completions_t* completions, const std::string& string);

  /**
   * Determine the path from the current node to the deepest descendant.
   * Assumes single descendancy only - not suitable for ParseNodes with
   * fully populated children!
   */
  virtual std::string descendants_path() const;

  /**
   * Determine the path to the current node, starting from the bottom,
   * and stopping at the indicated ancestor.  Unlike descendants_path(),
   * this API works on fat trees.
   */
  virtual std::string ancestor_path(ParseNode* stop) const;

  /**
   * Search for the parser node with the given path, which must be rooted
   * in this node.  The path string must start with '/'.  (Not const
   * member function because it should return a modifiable node).
   */
  virtual ParseNode* path_search(std::string path);

  /**
   * Create a XML node that corresponds to a parse node
   * ATTN: Does this need to change for value nodes?
   */
  virtual XMLNode* create_xml_node(XMLDocument *doc, XMLNode *parent);

  /**
   * Get an XML node - search for a node, and if not found, create it
   */
  virtual XMLNode* get_xml_node(XMLDocument *doc, XMLNode *parent);

  virtual CliCallback* get_callback();
  virtual void add_descendent(ParseNodeFunctional *desc);

  /**
   * Finds the descendent node given the node name.
   * @returns the ParseNode when found, nulltpr when not found
   */
  virtual ParseNodeFunctional* find_descendent(const std::string& name);

  /**
   * Finds the descendent node based on the given path. The path can be
   * something like this {"save", "config", "text"}.
   *
   * @returns the ParseNode when found, nulltpr when not found
   */
  virtual ParseNodeFunctional* find_descendent_deep(
                                  const std::vector<std::string>& path);


  virtual XMLNode* update_xml_for_leaf_list (XMLDocument *doc, XMLNode *parent, const char *attr);

  /**
   * Check if the parse node has cached application data.
   *
   * @return
   *  - true: The parse node has cached application data.
   *  - false: The parse node does not have cached application data.
   */
  virtual bool app_data_is_cached(
    /// [in] The parsing token for the data.
    const AppDataParseTokenBase& adpt
  ) const;

  /**
   * Check if the parse node has cached application data, and return
   * the data if it does.
   *
   * @return
   *  - true: The parse node has cached application data.
   *  - false: The parse node does not have cached application data.
   */
  virtual bool app_data_check_and_get(
    /// [in] The parsing token for the data.
    const AppDataParseTokenBase& adpt,

    /// [out] The cached data, if there is any.  May not be nullptr.
    intptr_t* data
  ) const;

  /**
   * Clear the app data vector.  Typically used when starting a parse,
   * after cloning the nodes.
   */
  virtual void app_data_clear();

  /**
   * Print a tree of the parser node and its descendants.
   */
  virtual void print_tree(unsigned indent, std::ostream& os) const;


  /**
   * Check if a parse node conflicts with a selected parse node. There is
   * no conflict detected if either of the parse nodes is NOT a Yang Parse node
   *
   * @return true if the nodes are from different branches of a YANG choice
   * @return false otherwise
   */

  virtual bool is_conflicting_node (
      ParseNode *other); /**< [in] the other node that is to be tested */

  /**
   * Check if for a list mode, the key keywords can be suppressed.
   * Works for no/show commands on the listy mode
   */
  virtual bool is_hide_key_keyword();

 public:
  /// Reference to the owning CLI instance.
  BaseCli& cli_;

  /// Pointer to the parent node. May be NULL.
  /// DOES NOT IMPLY this is in parent_->children_!
  ParseNode* parent_;

  /// Known children. Not necessarily complete, depending on node usage.
  children_t children_;

  /// The currently visible children (subset of children_).
  /// Not necessarily complete, depending on node usage.
  /// May be trimmed in some parsing contexts to specific kinds of children.
  /// ENTRIES MUST BE IN THE SAME RELATIVE ORDER AS children_!
  visible_t visible_;

  /// Parser control flags.
  ParseFlags flags_;

  /// The keyword. Not necessarily filled in or word-completed.
  std::string value_;

  /// The prompt string - is different from value for list nodes.
  std::string prompt_;

  /**
   * For each application data extension, this vector will contain the
   * nearest ancestor parse node, (including self) that has the
   * extension set, if any.  For any extension, if the vector slot is
   * unallocated or nullptr, then neither this node, nor any ancestor
   * node (to the mode root) has the extension set.  As new nodes are
   * added to the result tree, they inherit their parent's extensions,
   * which are then overriden with their own extensions, if any.
   *
   * The extensions set in the mode root will show up in this vector,
   * but extensions in the mode's ancestor node(s), if any, will not!
   *
   * Indexed by AppDataParseTokenBase::app_data_index.
   */
  std::vector<ParseNode*> app_data_found_;
};


/**
 * Parser Node for yang schema nodes.  This parser node is used for the
 * schema-tree nodes (non-values).
 */
class ParseNodeYang
: public ParseNode
{
 public:
  ParseNodeYang(BaseCli& cli, YangNode* yangnode, ParseNode* parent);
  ParseNodeYang(const ParseNodeYang& other, ParseNode* new_parent);
  virtual ~ParseNodeYang() {}

  ParseNodeYang(const ParseNodeYang& other) = delete;
  ParseNodeYang& operator=(const ParseNodeYang& other) = delete;

  virtual ptr_t clone(ParseNode* new_parent) const;

  bool is_clone(const ParseNode& other) const;
  bool is_clone_visit(const void* ocv, size_t size) const;

  bool is_leafy() const;
  bool is_key() const;
  bool is_config() const;
  bool is_rpc() const;
  bool is_rpc_input() const;
  bool is_notification() const;
  bool is_value() const;
  bool has_keys() const;
  bool is_mode() const;
  bool is_cli_print_hook() const;
  bool is_presence() const;
  bool is_keyword() const;
  bool is_sentence() const;
  bool is_mandatory() const override;
  bool is_suppressed() const override;
  std::vector<std::string> check_for_mandatory() const override;
  virtual bool is_mode_path() const;
  rw_yang_stmt_type_t get_stmt_type() const;
  void set_mode(const char *display);
  void set_cli_print_hook(const char *display);

  /**
   * @see ParseNode::set_suppressed()
   */
  void set_suppressed() override;

  ParseNode* mode_enter_top();
  const char* get_cli_print_hook_string();

  const char* token_text_get() const;
  const char* help_short_get() const;
  const char* help_full_get() const;
  const char* get_prefix();
  const char* get_ns();

  /**
   * ParseNodeYang override.
   * @see ParseNode::get_prompt_string
   */
  const char *get_prompt_string() override;

  /**
   * ParseNodeYang override.
   * @see ParseNode::value_is_match
   */
  bool value_is_match(const std::string& value, bool check_prefix) const;

  /**
   * Rebuild the list of visible children from the list of child key
   * nodes.
   */
  virtual void fill_visible_keys();

  /**
   * Rebuild the list of visible children from the list of child key
   * nodes.
   */
  virtual void fill_visible_nonkeys();


  void next(ParseLineResult* plr) override;
  void fill_children();
  void fill_rpc_input();
  void add_builtin_behavorial_to_visible();

  bool app_data_is_cached(const AppDataParseTokenBase& adpt) const override;
  bool app_data_check_and_get(const AppDataParseTokenBase& adpt, intptr_t* data) const override;
  virtual bool is_conflicting_node (ParseNode *other) override;

  /**
   * Check if for a list mode, the key keywords can be suppressed.
   * Works for no/show commands on the listy mode
   */
  virtual bool is_hide_key_keyword() override;

  /**
   * Specialized case for handling leaf-list nodes.
   * Leaf list nodes should also show its sibling nodes. If sibling nodes are
   * part of the completion, then the leaf-list node is poped out
   */
  void get_completions(completions_t* completions, const std::string& string) override;

 public:
  struct clone_visit_t {
    clone_visit_t(const ParseNodeYang* node)
    : yn(node->yangnode_)
    {}
    YangNode* yn;
  };

 public:
  /// Pointer to the schema model node.  May be NULL!
  YangNode* yangnode_;
  child_iter_t last_key_child_;
};


/**
 * Parser Node for yang value nodes.  This parser node is used for
 * leaf, leaf-list, and anyxml values.
 */
class ParseNodeValue
: public ParseNode
{
 public:
  ParseNodeValue(BaseCli& cli, YangNode* yangnode, YangValue* yangvalue, ParseNode* parent);
  ParseNodeValue(const ParseNodeValue& other, ParseNode* parent);
  virtual ~ParseNodeValue() {}

  ParseNodeValue(const ParseNodeValue& other) = delete;
  ParseNodeValue& operator=(const ParseNodeValue& other) = delete;

  ptr_t clone(ParseNode* new_parent) const;

  bool is_clone(const ParseNode& other) const;
  bool is_clone_visit(const void* ocv, size_t size) const;

  bool is_leafy() const;
  bool is_key() const;
  bool is_config() const;
  bool is_value() const;
  bool has_keys() const;
  bool is_mode() const;
  bool is_keyword() const;
  bool is_sentence() const;

  const char* token_text_get() const;
  const char* help_short_get() const;
  const char* help_full_get() const;

  const char* get_prefix() override;
  const char* get_ns() override;

  /**
   * Gets the yang type of the leaf value
   */
  rw_yang_leaf_type_t get_leaf_type() const;

  /**
   * ParseNodeValue override.
   * @see ParseNode::value_is_match
   */
  bool value_is_match(const std::string& value, bool check_prefix) const;
  const std::string& value_complete(const std::string& value);

  void next(ParseLineResult* plr) override;

 public:
  struct clone_visit_t {
    clone_visit_t(const ParseNodeValue* node)
    : yn(node->yangnode_),
      yv(node->yangvalue_)
    {}
    YangNode* yn;
    YangValue* yv;
  };

 public:
  /// Pointer to the schema model node.
  YangNode* yangnode_;

  /// Pointer to the schema value descriptor.
  YangValue* yangvalue_;
};


/**
 * Built-in parser node.  A parser node for keywords that do not exist
 * in the yang schema model.
 */
class ParseNodeBuiltin
: public ParseNode
{
 public:
  ParseNodeBuiltin(BaseCli& cli, const char* token_text, ParseNode* parent);
  ParseNodeBuiltin(const ParseNodeBuiltin& other, ParseNode* new_parent);
  virtual ~ParseNodeBuiltin() {}

  // no copying
  ParseNodeBuiltin(const ParseNodeBuiltin& other) = delete;
  ParseNodeBuiltin& operator=(const ParseNodeBuiltin& other) = delete;

  ptr_t clone(ParseNode* new_parent) const;

  bool is_clone(const ParseNode& other) const;
  bool is_clone_visit(const void* ocv, size_t size) const;

  bool is_leafy() const { return is_leafy_; }
  bool is_config() const { return is_config_; }
  bool is_mode() const { return is_mode_; }
  bool is_cli_print_hook() const { return is_cli_print_hook_; }
  bool is_sentence() const { return true; }
  bool is_built_in() const { return true; }
  ParseNode* mode_enter_top();

  const char* token_text_get() const;
  const char* help_short_get() const;
  const char* help_full_get() const;

  void next(ParseLineResult* plr) override;

  void help_short_set(const char* str);
  void help_full_set(const char* str);

  void set_is_leafy(bool v) { is_leafy_ = v; }
  void set_is_config(bool v) { is_config_ = v; }
  void set_is_mode(bool v) { is_mode_ = v; }
  void set_is_cli_print_hook(bool v) { is_cli_print_hook_ = v; }

 public:
  struct clone_visit_t {
    clone_visit_t(const ParseNodeBuiltin* node)
    : tt(node->token_text_)
    {}
    const char* tt;
  };

 public:
  /// The token string.
  const char* token_text_;

  /// The short help text.
  std::string help_short_;

  /// The full help text.
  std::string help_full_;

  bool is_leafy_;
  bool is_config_;
  bool is_mode_;
  bool is_cli_print_hook_;
};


/**
 * Functional Parser Node - These nodes decide the behavior of the the CLI,
 * and specify what types of nodes get displayed and completed.
 */
class ParseNodeFunctional
    : public ParseNode
{
 public:

  /**
   * Functional Node Constructor - build a node given the display text,
   * the flags that determine behavior, and the parent
   */
  ParseNodeFunctional(
      BaseCli& cli,               /**< The CLI object to which this node belongs */
      const char* token_text,     /**< Text to be displayed when this node is visible */
      ParseNode* parent);         /**< Parent Parsenode flag */

  /**
   * Constructor that clones the node with change of parent
   */
  ParseNodeFunctional(
      const ParseNodeFunctional& other, /**< node to copy */
      ParseNode* new_parent);         /**< parent of clone */

  ParseNodeFunctional(
      BaseCli& cli,               /**< The CLI object to which this node belongs */
      const ParseNodeFunctional& other, /**< node to copy */
      ParseNode* new_parent);         /**< parent of clone */

  /// Destructor
  virtual ~ParseNodeFunctional();

  /// no copy constructors
  ParseNodeFunctional(const ParseNodeFunctional& other) = delete;

  /// no copy constructors
  ParseNodeFunctional& operator=(const ParseNodeFunctional& other) = delete;

  /**
   * Build a clone of this node
   */

  virtual ParseNode::ptr_t clone(ParseNode* new_parent) const; /**< Parent of clone */

  /// Is this node a clone
  virtual bool is_clone(const ParseNode& other) const; /**< node to check against */

  /// Is the visit by a clone
  virtual bool is_clone_visit(const void* ocv, size_t size) const;


  bool is_leafy() const { return is_leafy_; }
  bool is_config() const { return is_config_; }
  bool is_mode() const { return is_mode_; }

  /// Does this functional node have a print hook string?
  bool is_cli_print_hook() const;

  /// The CLI print hook string
  const char* get_cli_print_hook_string();

  /// Set the CLI print hook string
  void set_cli_print_hook(const char *plugin);
  
  bool is_sentence()const;
  bool is_built_in() const { return false; }
  virtual ParseNode* mode_enter_top();
  virtual bool is_functional() const {return true;};

  const char* token_text_get() const;
  const char* help_short_get() const;
  const char* help_full_get() const;

  void next(ParseLineResult* plr) override;

  void help_short_set(const char* str);
  void help_full_set(const char* str);

  void set_is_leafy(bool v) { is_leafy_ = v; }
  void set_is_config(bool v) { is_config_ = v; }
  void set_is_mode(bool v) { is_mode_ = v; }
  void set_is_sentence (bool v) { is_sentence_ = v;}
  virtual void fill_children();
  virtual CliCallback* get_callback();
  virtual void set_value_node(ParseNodeValueInternal *value_node);
  public:
  struct clone_visit_t {
    clone_visit_t(const ParseNodeFunctional* node)
        : tt(node->token_text_)
    {}
    const char* tt;
  };

  /**
   * define permenant children of a parse node. These are cloned. Called
   * "descendents" since children is a field with different semantics in
   * base ParseNode class
   */

  typedef std::unique_ptr<ParseNodeFunctional> d_ptr_t;
  typedef std::list<d_ptr_t> descendent_t;
  typedef descendent_t::iterator desc_iter_t;
  typedef descendent_t::const_iterator desc_citer_t;

  /**
   * a type for a auto ptr to internal value node
   */
  typedef std::unique_ptr<ParseNodeValueInternal> v_ptr_t;

  /**
   * The functionals will register call backs, at the leafs
   */
  virtual void set_callback (CliCallback *cb);
  virtual void add_descendent (ParseNodeFunctional *desc);

  /**
   * @see ParseNode::find_descendent()
   */
  virtual ParseNodeFunctional* find_descendent(const std::string& name);

  /**
   * @see ParseNode::find_descendent_deep()
   */
  virtual ParseNodeFunctional* find_descendent_deep(
                                  const std::vector<std::string>& path);

 public:
  /// The token string.
  const char* token_text_;

  /// The short help text.
  std::string help_short_;

  /// The full help text.
  std::string help_full_;

  bool is_leafy_;
  bool is_config_;
  bool is_mode_;

  bool is_sentence_;
  descendent_t descendents_;
  CliCallback* cb_;
  v_ptr_t value_node_;
  
  /// print hook for cli commands
  std::string cli_print_hook_;

};

/**
 * Behavior Parser Node - These nodes decide the behavior of the the CLI,
 * and specify what types of nodes get displayed and completed.
 */
class ParseNodeBehavior
    : public ParseNodeFunctional
{
 public:
  /**
   * Behavior Node Constructor - build a node given the display text,
   * the flags that determine behavior, and the parent
   */
  ParseNodeBehavior(
      BaseCli& cli,               /**< The CLI object to which this node belongs */
      const char* token_text,     /**< Text to be displayed when this node is visible */
      ParseNode* parent);         /**< Parent Parsenode flag */

  /**
   * Constructor that clones the node with change of parent
   */
  ParseNodeBehavior(
      const ParseNodeBehavior& other, /**< node to copy */
      ParseNode* new_parent);         /**< parent of clone */

  ParseNodeBehavior(
      BaseCli& cli,               /**< The CLI object to which this node belongs */
      const ParseNodeBehavior& other, /**< node to copy */
      ParseNode* new_parent);         /**< parent of clone */

  /// Destructor
  virtual ~ParseNodeBehavior() {}

  /// no copy constructors
  ParseNodeBehavior(const ParseNodeBehavior& other) = delete;

  /// no copy constructors
  ParseNodeBehavior& operator=(const ParseNodeBehavior& other) = delete;

  /**
   * Build a clone of this node
   */

  ParseNode::ptr_t clone(ParseNode* new_parent) const; /**< Parent of clone */

  /// Is this node a clone
  bool is_clone(const ParseNode& other) const; /**< node to check against */

  /// Is the visit by a clone
  bool is_clone_visit(const void* ocv, size_t size) const;
  ParseNode* mode_enter_top();

  const char* token_text_get() const;
  const char* help_short_get() const;
  const char* help_full_get() const;

  void next(ParseLineResult* plr) override;

  virtual void fill_children(YangNode *yangnode);

 public:
  struct clone_visit_t {
    clone_visit_t(const ParseNodeBehavior* node)
        : tt(node->token_text_)
    {}
    const char* tt;
  };
};

/**
 * ParseNodeInternalValue - Some functional nodes need associated values - for example,
 * show cli manifest could take a manifest name to display, set cli history could use a
 * integer to limit the history buffer size
 */

class ParseNodeValueInternal
    : public ParseNode
{
 public:
  ParseNodeValueInternal(BaseCli& cli, rw_yang_leaf_type_t type, ParseNode* parent);
  ParseNodeValueInternal(const ParseNodeValueInternal& other, ParseNode* parent);
  ~ParseNodeValueInternal() {}

  ParseNodeValueInternal(const ParseNodeValueInternal& other) = delete;
  ParseNodeValueInternal& operator=(const ParseNodeValueInternal& other) = delete;

  ptr_t clone(ParseNode* new_parent) const;

  bool is_clone(const ParseNode& other) const;
  bool is_clone_visit(const void* ocv, size_t size) const;

  bool is_leafy() const;
  bool is_key() const;
  bool is_config() const;
  bool is_value() const;
  bool has_keys() const;
  bool is_mode() const;
  bool is_keyword() const;
  bool is_sentence() const;

  void help_short_set(const char* str);
  void help_full_set(const char* str);

  const char* token_text_get() const;
  const char* help_short_get() const;
  const char* help_full_get() const;

  /**
   * ParseNodeValueInternal override.
   * @see ParseNode::value_is_match
   */
  bool value_is_match(const std::string& value, bool check_prefix) const;
  const std::string& value_complete(const std::string& value);

  void next(ParseLineResult* plr) override;

 public:
  struct clone_visit_t {
    clone_visit_t(const ParseNodeValueInternal* node)
    : type(node->type_)
    {}
    rw_yang_leaf_type_t type;
  };

 public:
  rw_yang_leaf_type_t type_;

    /// The token string.
  const char* token_text_;

  /// The short help text.
  std::string help_short_;

  /// The full help text.
  std::string help_full_;

  bool is_leafy_;
  bool is_config_;
  bool is_mode_;
};


/**
 * Parse Line Result - a fully parsed line buffer.  The buffer is
 * parsed upon creation of the object.  The parsing success is kept in
 * the object.
 */
class ParseLineResult
{
 public:
  /**
   * Options to control the parser.
   */
  enum options_t
  {
    NO_OPTIONS    =    0, ///< No parser options specified.
    NO_COMPLETE   = 0x08, ///< Do not perform completion - words must exact match
    DASHES_MODE   = 0x10, ///< Allow leading dash on first word, fail if dashes later
    COMPLETE_LAST = 0x20, ///< Auto-complete the last word on the line, if possible

    /// Interactive-CLI mode
    ENTERKEY_MODE = COMPLETE_LAST,

    /// ARGV-processing mode
    ARGV_MODE = NO_COMPLETE|DASHES_MODE|COMPLETE_LAST,
  };

 public:
  /**
   * Create a Parse Line Result object from a string buffer, rooted in
   * the current mode.  The status of the parse is found in the success
   * member.
   */
  ParseLineResult(
    /// [in] The owning CLI.
    BaseCli& cli,

    /// [in] The command line to parse; will be tokenized into words.
    const std::string &line_buffer,

    /// [in] The parser options.  ATTN: convert to flags.
    int options
  );

  /**
   * Create a Parse Line Result object from a string vector, where each
   * string is an already-tokenized word.  The parse is rooted in the
   * current mode.  The status of the parse is found in the success
   * member.
   */
  ParseLineResult(
    /// [in] The owning CLI.
    BaseCli& cli,

    /// [in] The command line to parse, already tokenized.
    const std::vector<std::string>& words,

    /// [in] The parser options.  ATTN: convert to flags.
    int options
  );

  /**
   * Create a Parse Line Result object from a string buffer, rooted in
   * the specified mode.  The status of the parse is found in the
   * success member.
   */
  ParseLineResult(
    /// [in] The owning CLI.
    BaseCli& cli,

    /// [in] The root mode for the parse.
    ModeState& mode,

    /// [in] The command line to parse; will be tokenized into words.
    const std::string& line_buffer,

    /// [in] The parser options.  ATTN: convert to flags.
    int options
  );

  // cannot copy
  ParseLineResult(const ParseLineResult&) = delete;
  ParseLineResult& operator=(const ParseLineResult&) = delete;

 private:
  /**
   * Parse a command line string.  Tokenize the string before parsing.
   */
  void parse_line();

  /**
   * Parse the command line vector.  ATTN: This should be a public
   * method that the caller calls explicitly, rather than being called
   * implicitly from the constructor.
   */
  void parse_line_words();

  /**
   * Check the current result node for application data extensions and
   * corresponding callbacks.
   *
   * @return
   *  - true: Continue the current parse.
   *  - false: Stop the current parse and fail the command line.
   *  - ATTN: This should return a better error status
   */
  bool handle_app_data();

  /**
   * Sets the current subtree and add's new nodes based on the
   * completions or node type
   */
  void adjust_subtrees(ParseNode* parse_found);

 public:
  /**
   * Check the result for cached application data.  If any parse node
   * in the result had cached the application data, return the first
   * parse node that did.
   *
   * @return
   *  - true: The value was cached by at least one parse node.
   *  - false: The value was not cached by any parse node.
   */
  bool app_data_is_cached_first(
    /// [in] The parsing token for the data.
    const AppDataParseTokenBase& adpt,

    /// [out] The first parse node that had cached data.
    /// Cannot be nullptr.
    ParseNode** parse_node
  ) const;

  /**
   * Check a parse line result for this application data.  If any parse
   * node in the result had cached the application data, return the
   * last parse node that did.
   *
   * @return
   *  - true: The value was cached by at least one parse node.
   *  - false: The value was not cached by any parse node.
   */
  bool app_data_is_cached_last(
    /// [in] The parsing token for the data.
    const AppDataParseTokenBase& adpt,

    /// [out] The last parse node that had cached data.
    /// Cannot be nullptr.
    ParseNode** parse_node
  ) const;

  /**
   * Check a parse line result for the first node that had cached this
   * application data.  If a parse node had the application data,
   * return the data and (optionally) the node.
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  bool app_data_check_and_get_first(
    /// [in] The parsing token for the data.
    const AppDataParseTokenBase& adpt,

    /// [out] The first parse node that had cached data, if any.  May
    /// be nullptr if the node is not wanted.
    ParseNode** parse_node,

    /// [out] The data value, if cached.  Cannot be nullptr.
    intptr_t* data
  ) const;

  /**
   * Check a parse line result for the last node that had cached this
   * application data.  If a parse node had the application data,
   * return the data and (optionally) the node.
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  bool app_data_check_and_get_last(
    /// [in] The parsing token for the data.
    const AppDataParseTokenBase& adpt,

    /// [out] The last parse node that had cached data, if any.  May
    /// be nullptr if the node is not wanted.
    ParseNode** parse_node,

    /// [out] The data value, if cached.  Cannot be nullptr.
    intptr_t* data
  ) const;

 public:
  /// Reference to the owning CLI instance.
  BaseCli& cli_;

  /// The complete parsing tree.  Nodes in this tree may have all
  /// their children populated and their visible children lists
  /// manipulated.  The parser may backtrack up this tree.
  ParseNode::ptr_t parse_tree_;

  /// The current parser node in parse_tree.  The user may refer to
  /// this node after parsing.
  ParseNode* parse_node_;

  /// The current list of completions.  These nodes are all in
  /// parse_tree_.  After parsing completes, this list will not
  /// necessarily be the list of completions for parse_node_!
  ParseNode::completions_t completions_;

  /// The nodes visited by the parser.  Not necessarily single
  /// descendent path.  May contain multiple clones of the same
  /// nodes, for leaf and leaf list types.  These nodes are all
  /// clones of visited nodes in parse_tree_, generally in the order
  /// they were visited.
  ParseNode::ptr_t result_tree_;

  /// The result_tree_ clone of the current parse_node_.  This allows
  /// the parser to walk result_tree_ in parallel with parse_tree_.
  ParseNode* result_node_;

  /// If not null, then entering a new mode is requested, root at the
  /// indicated node.
  ParseNode* result_mode_top_;

  /// The line buffer being parsed.
  std::string line_buffer_;

  /// The split words, as they appeared on the command line, possibly
  /// completed.
  std::vector<std::string> line_words_;

  /// The completed buffer, with all words expanded.
  std::string completed_buffer_;

  /// The last word parsed.  May be referred to by the user.
  std::string last_word_;

  /// The parsing options
  int parse_options_;

  /// The line ended with a space
  bool line_ends_with_space_;

  /// The parsing was successful
  bool success_;
  /// The error string describing why the parsing failed
  std::string error_;

  // ATTN: Convert to app_data
  /// The most recent cli_print_hook_string
  const char* cli_print_hook_string_;

  // ATTN: Convert to app_data
  /// The most recent callback function set
  CliCallback*  cb_;

  /**
   * For each application data extension, this vector contains the
   * first parse node, in the result tree, that was found with the
   * extension set in the corresponding yang node.  The entry is set
   * the first time a parse node is visited.  Once set, an entry will
   * not be changed during a particular parse.  If any particular entry
   * is nullptr or if the vector is too small, the extension has not
   * been found.
   *
   * Indexed by AppDataParseTokenBase::app_data_index and/or
   * AppDataParseToken<>::app_data_index.
   */
  std::vector<ParseNode*> app_data_first_;

  /**
   * For each application data extension, this vector contains the most
   * recent parse node, in the result tree, that was found with the
   * extension set in the corresponding yang node.  An entry will
   * change every time the parser visits such a parse node, even if the
   * parse node is visited multiple times.  If any particular entry is
   * nullptr or if the vector is too small, the extension has not been
   * found.
   *
   * Indexed by AppDataParseTokenBase::app_data_index and/or
   * AppDataParseToken<>::app_data_index.
   */
  std::vector<ParseNode*> app_data_last_;

};


/**
 * RW XML unprocessed node stack. Used to keep nodes that are unprocessed when
 * a result tree is merged to a xml node at the mode level
 */

class XmlResultMergeState {
 public:
  ParseNode *pn_;                 /**< the node to be processed */
  XMLNode *xn_;           /**< the XML node at which the result node needs to be merged */

  XmlResultMergeState (ParseNode *pn, XMLNode *xn)
      : pn_ (pn),
        xn_ (xn)
  {};

  typedef std::deque <XmlResultMergeState > stack_t;
  typedef stack_t::iterator iter_t;
  typedef stack_t::size_type size_t;
};

/**
 * Mode stack state.  These data structures keep track of which mode
 * the parser is in and where in the schema the modes are rooted.
 */
class ModeState
{
  public:
  ModeState(BaseCli& cli, const ParseNode& root_node, XMLNode *dom_node = 0);
    ModeState(ParseLineResult* r, XMLNode *dom_node = 0);
    ModeState(ParseNode::ptr_t&& result_tree,
              ParseNode* result_node, XMLNode *dom_node = 0);

    virtual ~ModeState() {}

    ModeState(const ModeState&) = delete;            ///< Not copyable.
    ModeState& operator=(const ModeState&) = delete; ///< Not assignable.

    void set_xml_node (XMLNode *node);
    XMLNode* update_xml_cfg (XMLDocument *doc, rw_yang::ParseNode::ptr_t &result_tree);
    XMLNode* update_xml_cfg (rw_yang::ParseNode *pn, XMLNode *parent_xml);
    XMLNode* create_xml_node(XMLDocument *doc, XMLNode *parent_xml);
    XMLNode *merge_xml (XMLDocument *doc, XMLNode *parent_xml, ParseNode *pn);
    ParseNode::ptr_t clone_top_parse_node();

    /**
     * Sets the node with a IdentityRef value. IdentityRef's are special values
     * which can have namespace prefix specified in the xml text field.
     */
    void set_idref_value(XMLNode *node, ParseNodeValue* value);

  public:
    typedef std::unique_ptr<ModeState> ptr_t;
    typedef std::vector<ptr_t> stack_t;
    typedef stack_t::iterator stack_iter_t;
    typedef stack_t::reverse_iterator reverse_stack_iter_t;

  public:
    /// Reference to the owning CLI instance.
    BaseCli& cli_;

    /// The complete parsing path that entered the mode
    ParseNode::ptr_t mode_parse_path_;

    /// The top node for the current mode (deepest node in mode_parse_path).
    ParseNode* top_parse_node_;

    /// XML node corresponding to the top node for the current node
    XMLNode *dom_node_;   /**< Location in the configuration DOM */
};


/**
 * Base CLI implementation.  This is not a concrete class - you must
 * inherit from this class and implement at least handle_command().
 * This class provides the interface (and basic implementation) that
 * the other classes use to interact with the CLI.
 */
class BaseCli
{
  public:
  /**
   * Is the base CLI in a particular state or mode? Only config for now.
   */
  enum state_t
  {
    NONE = 0,
    STATE_base = 0x57A7EBA5E,
    STATE_CONFIG,
    STATE_end,

    FIRST = STATE_base+1,
    LAST = STATE_end - 1,
  };

  static const unsigned MAX_MODE_STACK_DEPTH = 64;
  static const size_t MAX_PROMPT_LENGTH = 128;
  static bool is_state_good (state_t v) {return (int) v > (int) FIRST && (int) v <= (int) LAST;}
  static bool is_good_or_none(state_t v) { return    v == NONE || (    (int)v >= (int)FIRST && (int)v <= (int)LAST); }

  typedef AppDataParseToken<
    const char*,
    AppDataTokenDeleterNull
  > cliext_adpt_t;

 public:
  BaseCli(YangModel& model);
  virtual ~BaseCli();

  void add_builtins();
  void add_behaviorals();
  virtual  void enhance_behaviorals(){};

  virtual void setprompt();
  virtual void mode_end();
  virtual void mode_end_even_if_root();
  virtual void mode_exit();
  virtual void mode_pop_to(ModeState* mode);
  virtual void mode_enter(ParseLineResult* r);
  virtual void mode_enter(ParseNode::ptr_t&& result_tree, ParseNode* result_node);
  virtual void mode_enter_impl(ModeState::ptr_t new_mode);
  virtual void mode_stack_root(ModeState::ptr_t new_root);

  /**
   * Invoked when config mode is exited by issuing either 'exit' or 'end'
   *
   * Derived classes can implement custom behavior on config mode exit
   *
   * @returns false to cancel config mode exit, true otherwise 
   */
  virtual bool config_exit();

  // ATTN: This return value is lame.
  virtual bool handle_command(ParseLineResult* r) = 0;

  // ATTN: TBD: FUTURE: Should have ParseNode factory.
  // ATTN: TBD: FUTURE: Should have ParseLineResult factory.
  // ATTN: TBD: FUTURE: Should have ModeState factory.

  // ATTN: This return value is lame.
  virtual bool generate_help(const std::string& line_buffer);

  /**
   * Generates a vector of matches for the given command in the line_buffer.
   * For use with tab_complete and help generation. Returns the matches as a
   * vector unlike generate_help or tab_complete which prints to stdout.
   * @param[in] line_buffer  command for which completions to be returned
   * @return
   *  Returns a vector of matches
   */
  virtual std::vector<ParseMatch> generate_matches(const std::string& line_buffer);

  virtual std::string tab_complete(const std::string& line_buffer);
  /**
   * Does some preprocessing on the given command in the line buffer before
   * calling 'parse_line_buffer'.
   * The preprocessing takes caring of
   * 1. Putting/wrapping a command word with quotes '"' whenever require.
   * 2. Escaping special characters or unprintable/unicaode characters.
   * The processed command is then fed to 'parse_line_buffer' function.
   */
  virtual parse_result_t process_line_buffer(int argc, const char* const* argv, bool interactive);
  // ATTN: This return value is lame.
  virtual parse_result_t parse_line_buffer(const std::string& line_buffer, bool interactive);
  virtual unsigned parse_argv(const std::vector<std::string>& argv);
  virtual bool parse_argv_one(int argc, const char* const* argv);
  virtual bool parse_argv_set(int argc, const char* const* argv);

  virtual ParseNode::ptr_t clone_current_mode_parse_node();
  virtual void add_top_level_functional (ParseNodeFunctional *node);

  /**
   * Handle an error in parsing. The base class function does nothing,
   * and the derived classes have to implement handling
   */
  virtual void handle_parse_error(const parse_result_t, const std::string);
  bool show_behavioral (const ParseLineResult& r );

  /**
   * Invoked when CLI enters the config mode.
   * @return true on successful completion, false otherwise
   */
  virtual bool config_behavioral (const ParseLineResult& r);

  virtual bool no_behavioral (const ParseLineResult& r);
  virtual bool show_config (const ParseLineResult& r);
  virtual bool show_candidate_config (const ParseLineResult& r);
  virtual bool show_operational (const ParseLineResult& r);
  virtual bool commit_behavioral (const ParseLineResult& r);
  virtual bool discard_behavioral (const ParseLineResult& r);

  virtual bool process_config (const ParseLineResult& r);

  /**
   * Register an application data extension and allocate a parser index
   * for it.  If the same extension is registered twice, the same
   * parser index will be returned.  In that case, all registrants must
   * set the same callback, or nullptr.
   *
   * @return A copy of the parser token.
   */
  virtual AppDataParseTokenBase app_data_register(
    /**
     * [in] The namespace of the extension.  Doesn't have to be a
     * namespace of a loaded yang module, in which case only extensions
     * that are explicitly set in the yang node would be found.
     */
    const char* ns,

    /// [in] The name of the extension.
    const char* ext,

    /// [in] The yang token deleter.
    AppDataTokenBase::deleter_f deleter,

    /**
     * [in] The parser callback to make, during parse, each time the
     * extension is found on a result-tree parse node that has been
     * visited.  This callback should be used ONLY to modify the parser
     * behaviour, such as by setting parse node flags.  DO NOT USE THIS
     * FOR APPLICATION CALLBACKS!  The parse is not complete when this
     * gets called, so you can't know if the command is complete or
     * even valid.
     */
    AppDataCallback* callback = nullptr
  );

  /**
   * For the given parse completion entry, returns the token with prefix if
   * prefix_required is set on the entry
   */
  std::string get_token_with_prefix(
    /// [in] Completion entry that matched the last word 
    const ParseCompletionEntry& entry
  );

  /**
   * Given the functional command path, returns the ParseNode of the last token.
   * Returns null when no match is found.
   */
  ParseNodeFunctional* find_descendent_deep(
                        const std::vector<std::string>& path);

  /**
   * Suppress all parse nodes in the given namespace.
   *
   * @returns true on success, false when the namespace is not found.
   */
  bool suppress_namespace(const std::string& ns);

  /**
   * Suppress the parse node that starts with the given path
   *
   * @returns true on success, false when the path does not exist.
   */
  bool suppress_command_path(const std::string& path);

 public:
  /// The schema model, which defines the (majority of the) CLI grammar.
  YangModel& model_;

  /// The current stack of CLI modes.
  ModeState::stack_t mode_stack_;

  /// The root mode.  Lives until the CLI is destroyed
  ModeState* root_mode_;

  /// The current mode.
  ModeState* current_mode_;

  /// CLI prompt
  char prompt_[MAX_PROMPT_LENGTH];

  /// static parse node for "exit"
  ParseNode::ptr_t builtin_exit_;

  /// static parse node for "end"
  ParseNode::ptr_t builtin_end_;

  /// Root yang parse node
  ParseNode::ptr_t root_parse_node_;

  /// State of the CLI.
  state_t state_;

  /// Behavior nodes. Should these be a vector?

  /// "show" node
  ParseNode::ptr_t show_node_;

  /// "no" node
  ParseNode::ptr_t no_node_;

  /// "mode" node - follows a path to a mode if it exists - not otherwise
  ParseNode::ptr_t mode_node_;

  //// config node - One step to a mode, and thats it
  ParseNode::ptr_t config_node_;

  /// Support for 'commit' and 'rollback'
  bool has_candidate_store_ = false;

  /// "commit" node
  ParseNode::ptr_t commit_node_;

  /// "discard" node
  ParseNode::ptr_t discard_node_;

  /// "config" node to show running config
  ParseNode::ptr_t show_config_node_;

  /// "candidate-config" node to show candidate config
  ParseNode::ptr_t show_candidate_node_;

  /// Functional nodes that need to be added to the CLI ROOT
  // each entry represents a top level function of the CLI
  ParseNodeFunctional::descendent_t top_funcs_;

  /**
   * The list of registered application data extensions.
   *
   * Indexed by AppDataParseTokenBase::app_data_index.
   */
  std::vector<AppDataParseTokenBase> app_data_tokens_;

  cliext_adpt_t ms_adpt_;
  cliext_adpt_t ph_adpt_;

  /// To handle the CLI extension "show-key-keyword"
  cliext_adpt_t skk_adpt_;

  /// App Data set on YangNode for supressing the keyword
  cliext_adpt_t suppress_adpt_;
};

/**
 * Callback function implemenation. A abstract base class with a call method. Classes
 * inherited from this class will implement the right callbacks
 */

class CliCallback {
 public:
  CliCallback(){};
  virtual ~CliCallback(){};
  /**
   * The execute function is the main member function. As this interface is used after
   * parsing completes, the result of the parsing is always the result
   */
  virtual bool execute (
      const ParseLineResult& r) = 0;  /**< [in] The line that was parsed by CLI */
};


/**
 * A pointer to a member function of Base CLI that takes as an argument a ParseLine result and
 * returns a boolean
 */

typedef bool (BaseCli::*BaseCliCallback) (const ParseLineResult& r);

/**
 * A callback class that works on base CLI. Data stored is the object on which a
 * function is to be called, and a member function pointer
 */

class CallbackBaseCli : public CliCallback {
 public:
  BaseCliCallback   cb_func_;  /**< The function to be called when execute is called */
  BaseCli           *cb_data_;  /**< Object on which the callback is to be executed */

 public:
  CallbackBaseCli(BaseCli *data, BaseCliCallback func);
  virtual ~CallbackBaseCli() {};
  CallbackBaseCli(const CallbackBaseCli&) = delete;                 // not copyable
  CallbackBaseCli& operator = (const CallbackBaseCli&) = delete;  // not assingnable
  /**
   * The execute function is the main member function. As this interface is used after
   * parsing completes, the result of the parsing is always the result
   */

  virtual bool execute(
      const ParseLineResult& r); /**< results of parsing the CLI input */

};


#define RW_BCLI_SYNTAX_CHECK(pred_,args_...) \
  do { \
    if(!(pred_)) { /*ATTN:rw_branch_predict*/ \
      std::cout << args_ << std::endl; \
      return false; \
    } \
  } while(0)

#define RW_BCLI_MIN_ARGC_CHECK(r_,cnt_) \
  RW_BCLI_SYNTAX_CHECK( (r_)->line_words_.size() >= (cnt_), "Too few arguments" )

#define RW_BCLI_ARGC_CHECK(r_,cnt_) \
  RW_BCLI_SYNTAX_CHECK( (r_)->line_words_.size() == (cnt_), "Wrong number of arguments, expected " << (cnt_) )

#define RW_BCLI_RW_STATUS_CHECK(rs_) \
  RW_BCLI_SYNTAX_CHECK( (rs_) == RW_STATUS_SUCCESS, "Failed: "<<(rs_) )



template <class data_type, class deleter_type>
inline AppDataParseToken<data_type,deleter_type>
AppDataParseToken<data_type,deleter_type>::create_and_register(
  const char* ns,
  const char* ext,
  BaseCli* parser,
  AppDataCallback* callback)
{
  RW_ASSERT(parser);
  AppDataParseTokenBase base
   = parser->app_data_register(ns, ext, &deleter_type::deleter, callback);
  return *static_cast<AppDataParseToken*>(&base);
}

template <class data_type, class deleter_type>
inline bool AppDataParseToken<data_type,deleter_type>::parse_node_check_and_get(
  ParseNode* parse_node,
  data_type* data) const
{
  intptr_t intptr_data = 0;
  RW_ASSERT(parse_node);
  if (parse_node->app_data_check_and_get(*this, &intptr_data)) {
    RW_ASSERT(data);
    *data = reinterpret_cast<data_type>(intptr_data);
    return true;
  }
  return false;
}

template <class data_type, class deleter_type>
inline bool AppDataParseToken<data_type,deleter_type>::parse_result_check_and_get_first(
  const ParseLineResult* plr,
  ParseNode** parse_node,
  data_type* data) const
{
  intptr_t intptr_data = 0;
  RW_ASSERT(plr);
  if (plr->app_data_check_and_get_first(*this, parse_node, &intptr_data)) {
    RW_ASSERT(data);
    *data = reinterpret_cast<data_type>(intptr_data);
    return true;
  }
  return false;
}

template <class data_type, class deleter_type>
inline bool AppDataParseToken<data_type,deleter_type>::parse_result_check_and_get_last(
  const ParseLineResult* plr,
  ParseNode** parse_node,
  data_type* data) const
{
  intptr_t intptr_data = 0;
  RW_ASSERT(plr);
  if (plr->app_data_check_and_get_last(*this, parse_node, &intptr_data)) {
    RW_ASSERT(data);
    *data = reinterpret_cast<data_type>(intptr_data);
    return true;
  }
  return false;
}

template <class data_type, class deleter_type>
inline const typename AppDataParseToken<data_type,deleter_type>::yang_token_t*
AppDataParseToken<data_type,deleter_type>::get_yang_token() const
{
  return static_cast<const yang_token_t*>(&ytoken);
}


} /* namespace rw_yang */

#endif // RW_YANGTOOLS_BASECLI_HPP_

/** @} */
