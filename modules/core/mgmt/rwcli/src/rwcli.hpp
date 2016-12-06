
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
 * @file rwcli.hpp
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @author Vinod Kamalaraj (vinod.kamalaraj@riftio.com)
 * @date 03/18/2014
 * @brief Top level include for RIFT cli
 */

#ifndef __RWCLI_HPP__
#define __RWCLI_HPP__

#if defined(__cplusplus) || defined(c_plusplus) || defined (__doxygen__)

#if __cplusplus < 201103L
#error "Requires C++11"
#endif



#include "yangncx.hpp"
#include "yangcli.hpp"
#include "rw_xml.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "util.hpp"
#include "rwcli.h"
#include "rwcli_zsh.h"
#include "rwcli_schema.hpp"

#define CLI_PRINTF printf

using namespace rw_yang;

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>


#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

/**
 * Forward declarations
 */

class RwCLI;     /**< An abstraction of an instance of a CLI  */
class RwCliParser; /**< A line of command to be parsed */
class CallbackRwCli; /**< A callback object to attach to parse nodes */

/**
 * CLI Parser node. This class provides the functionality to interface with a
 * command line
 *
 * The Parser is owned by the RwCLI object. The functionality to interface with
 * a terminal, and to parse input from a terminal according to loaded YANG schema
 * is provided by the BaseCLI class.
 */
class RwCliParser: public BaseCli {

  typedef std::unique_ptr<ParseNode> p_ptr_t;
  typedef std::list<p_ptr_t> functionals_t;

  typedef std::unique_ptr<CallbackRwCli> c_ptr_t;
  typedef std::list<c_ptr_t> callbacks_t;

 public:
  /**
   * the constructor that CLI "usually" uses.
   */
  RwCliParser(
      YangModel& model,      /**< [in] The YANG Model to operate on */
      RwCLI& rw_cli);        /**< [in] Owning CLI object */

  /**
   * Constructor used for a "temperory" parse, used to get to a yang node in
   * the model that corresponds to a particular path.
   */

  RwCliParser(
      YangModel& model,          /**< [in] The YANG Model to operate on */
      RwCLI& owner,              /**< [in] Owning CLI object */
      ParseFlags::flag_t flag);  /**< [in] Flags to set on the top parse node */

  /**
   * Handle the base-CLI command callback.  This callback will receive
   * notification of all syntactically correct command lines.  It should
   * return false if there are any errors.
   */
  bool handle_command(
      ParseLineResult* r); /**< [in] Parsed input, YANG/Internal Parse Nodes and
                              tree */

  /**
   * Handle errors reported @see BaseCli::handle_parse_error()
   */
  void handle_parse_error (const parse_result_t error,
                           const std::string error_string);
  /**
   * Handle the base-CLI mode_enter callback.
   */
  void mode_enter(
      ParseLineResult* r); /**< [in] Parsed input, YANG/Internal Parse Nodes and
                              tree */

  bool config_exit() override;

  /**
   * Handle the base-CLI mode_enter callback.
   */

  void mode_enter(
      ParseNode::ptr_t&& result_tree,  /**< [in] Parsed input, YANG/Internal
                                          Parse Nodes and tree */
      ParseNode* result_node);         /**< [in] Result node of Parse Tree */

  /**
   * From the Yang Model in use, and the data entered by the CLI user, generate
   * a DOM. The DOM is then used to generate XML to send to the Management Agent
   * for commands that require the interaction
   */

  rw_yang::XMLDocument::uptr_t get_command_dom (
      ParseNode* result_node);   /**< [in] The result node of the parsing that
                                  requires the DOM to be generated*/

  /**
   * Associate the default print hook with appropriate behavioral nodes
   */
  void associate_default_print_hook();

  /**
   * Associate the config print hook with config behavioral nodes 
   */
  void associate_config_print_hook();

  /**
   * Associate the config file printing hook with appropriate behavioral nodes
   */
  void associate_config_print_to_file_hook();

  /**
   * Associate the pretty printing XML hook with appropriate behavioral nodes
   */
  void associate_pretty_print_xml_hook();

  /**
   * Associate the pretty printing XML to file hook with appropriate behavioral
   * nodes
   */
  void associate_pretty_print_xml_to_file_hook();

  /**
   * Functional ParseNodes are created for yang-like nodes that provide syntax
   * to the CLI. The base functionals of show, config and no are created by the
   * BaseCLI. This function adds subnodes to these nodes, to create parse trees
   * for commands like show configuration.
   */
  virtual void enhance_behaviorals();

  /**
   * The show parse node is created in the base CLI. When "show" is entered
   * in configuration mode, this function is the callback in the base cli.
   * Over riding the function.
   */
  virtual bool show_config (
      const ParseLineResult& r);  /**< [in] Parsed input, YANG/Internal Parse
                                     Nodes and tree */

  /**
   * Show candidate configuration
   */
  bool show_candidate_config(const ParseLineResult& r) override;


  /**
   * The show parse node is created in the base CLI. When "show" is entered
   * this function is the callback in the base cli.
   * Over riding the function.
   */
  virtual bool show_operational (
      const ParseLineResult& r);  /**< [in] Parsed input, YANG/Internal Parse
                                     Nodes and tree */

  /**
   * Invoked when CLI entere the config mode.
   * @return true on successful completion, false otherwise
   */
  virtual bool config_behavioral (
                  /// [in] Parsed input, YANG/Internal parse nodes and tree
                  const ParseLineResult& r) override;

  /**
   * The show parse node is created in the base CLI. When "show" is entered
   * this function is the callback in the base cli.
   * Over riding the function.
   */
  virtual bool no_behavioral (
      const ParseLineResult& r);  /**< [in] Parsed input, YANG/Internal Parse
                                     Nodes and tree */

  /**
   * This method is invoked when a 'commit' command is entered.
   */
  virtual bool commit_behavioral (
      const ParseLineResult& r) override;  /**< [in] Parsed input, YANG/Internal Parse
                                     Nodes and tree */

  /**
   * This method is invoked when a 'commit' command is entered.
   */
  virtual bool discard_behavioral (
      const ParseLineResult& r) override;  /**< [in] Parsed input, YANG/Internal Parse
                                     Nodes and tree */
  /**
   * When a valid command is entered in the CLI, and the mode of the CLI is set
   * to configure, this function is called in the baseCLI. overriding the
   * function.
   */

  virtual bool process_config (
      const ParseLineResult& r);  /**< [in] Parsed input, YANG/Internal Parse
                                     Nodes and tree */

 public:
  RwCLI& rw_cli;                /**< The CLI that owns this parser */
  functionals_t functionals_;   /**< Functional nodes that have been added by
                                   this parser */
  callbacks_t callbacks_;       /**< Callback functions used by this parser */

  bool uncommitted_changes_ = false;
  bool auto_commit_ = false;
};


/**
 * The CLI manifest is a file in XML format that controls the behavior of the
 * CLI. The manifest contains the list of YANG modules to load, the paths at
 * which the CLI should enter different modes, plug ins used by the CLI etc.
 * This class abstracts the manifest. The manifests contain:
 *  - names of yang modules that are loaded
 *  - plugin extension map
 *  - plugin repository map
 *  - name spaces
 *  - CLI modes
 */

class CliManifest {
  struct ReadingStatus {
    rw_status_t status;
    std::string message;
    ReadingStatus(rw_status_t s,
                  std::string m)
        :status(s),
         message(m)
    {}
    ReadingStatus(rw_status_t s)
        :status(s),
         message("")
    {}
  };

 public:

  typedef std::set<std::string>         mfiles_t;
  typedef std::vector<std::string>      string_list_t;
  typedef std::map<std::string, void *> plugin_ptr_map_t;
  typedef std::map<std::string, std::string> name_map_t;


  mfiles_t         mfiles;               /**< List of manifest files loaded */
  plugin_ptr_map_t plugin_iface_map;     /**< Plugin interface pointer map */
  plugin_ptr_map_t plugin_klass_map;     /**< Plugin klass pointer map */
  name_map_t       plugin_repository_map;/**< Plugin repository */
  string_list_t    namespaces;           /**< Namespaces */
  name_map_t       cli_modes;            /**< CLI modes */
  name_map_t       cli_print_hooks;      /**< CLI print hooks */
  RwCLI&           rw_cli;               /**< The owner of this manifest */


  /**
   * Constructor for CLI manifest
   */
  explicit CliManifest(RwCLI &rw_cli);

  /**
   * Displays the manifest of the CLI - namespaces, plugins, additional modes
   */
  rw_status_t show();

  /**
   * Add a yang module to the parser schema
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE on failure
   */
  rw_status_t add_yang_module(char *module); /**< [in] name of the module */

  /**
   * A "print hook" enables the text based display of CLI output to be generated
   * by a function that is provided. The print hook can be added to any node
   * in the schema. If the CLI command uses the node in the schema, the print
   * hook is called to render any output

   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE on failure
   */
  ReadingStatus add_cli_print_hook(
      const char *path,  /**< Path to the node which needs a new print method */
      const char *hook); /**< The name of the function that is the print hook */


  /**
   * Add a plug in to the manifest, and apply it to rw_cli
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE on failure
   */
  rw_status_t add_plugin(
      char *plugin,    /**< name of the plugin */
      char *typelib);  /**< typelib */


  /**
   * Add a new mode to the cli
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE on failure
   */
  ReadingStatus add_cli_mode(
      const char *mode,      /**< [in] Path which becomes a new mode */
      const char *display);  /**< [in] The string that gets added to the prompt
                              at the new mode*/


  /**
   * Read a XML manifest file and parse the XML. for each entry, create a
   * plugin, add a mode, specify a print hook etc, as the case may be.
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE on failure
   */
  rw_status_t read_cli_manifest(const char* name);

  /**
   * Reads the base CLI Manifest file (rw.cli.xml).
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE on failure
   */
  rw_status_t read_base_cli_manifest(
                /// [in] Base CLI Manifest File path
                const char* file);

  /**
   * Read the common parts of the CLI Manifest document. Common parts include
   * plugin-list, yang-modules, namespace-list, modifications.
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE on failure
   */
  rw_status_t read_cli_manifest_common(
                  /// [in] CLI Manifest XML Doc
                  xmlDoc *doc);

  /**
   * Read the defaults section of the CLI Manifest
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE on failure
   */
  rw_status_t read_cli_manifest_defaults(
                  /// [in] CLI Manifest XML Doc
                  xmlDoc *doc);

};

class RwCLI
: public rwcli_s
{
 public:
  RwCLI(
      rwcli_transport_mode_t transport_mode,
      rwcli_user_mode_t user_mode);
  ~RwCLI();
  explicit RwCLI(RwCLI *other);

  /**
   * Initialize the RwCLI. Reads the base cli manifest, add builtins etc..
   */
  void initialize(
          const char* base_manifest_file); /**< [in] Base Manifest file path*/

  void add_builtins(void);

  void setprompt(void);

  void commit(void);

  rw_status_t initialize_plugin_framework();
  bool handle_command(ParseLineResult* r);

  /**
   * Some CLI commands require interaction with a MgmtAgent. This function handle
   * sending the request and processing the responses
   */

  rw_status_t process_cli_command(
      char *command,        /**< XML of the command to be processed */
      NetconfRsp **rsp,     /**< response from MgmtAgent */
      NetconfOperations op = nc_default, /*!< Netconf operation */
      NetconfEditConfigOperations = ec_merge); /**< EditConfig operation */


  bool process_cli_command (const ParseLineResult& r,
                            NetconfOperations op = nc_default,
                            NetconfEditConfigOperations = ec_merge);


  /**
   * Processes the rpc response message received from the management server.
   * Pretty print the output and also checks for errors.
   *
   * @returns false on error response.
   */
  bool process_rpc_response(
        const ParseLineResult& r, /**< Parsed Command result */
        NetconfOperations op, /**< Netconf operation performed */
        NetconfEditConfigOperations ec_op, /**< EditConfig operation */
        NetconfRsp *rsp); /**< Netconf response */

  /**
   * Returns true if the XML response is an error response
   */
  bool is_error_response(const char* xml_str /**< Response XML String */);


  rw_status_t print_hook(const ParseLineResult &r, NetconfRsp *rsp);

  /**
   * Response print handling handled by RwCLI and not delegated to a print
   * plugin.
   */
  bool print_hook_self(const ParseLineResult &r, 
            NetconfRsp *rsp,
            const std::string& routine);

  void print_element_names(xmlNode *node, int indent);

  void display_xml_response(char *response);


  /**
   * Save a configuration to XML or text. Hooked to the CLI callback mechanism.
   * XML or text is based on the cli print hook defined.
   */
  bool save_config(const ParseLineResult& r);

  /**
   * Show a configuration file as text. Hooked to the CLI callback mechansim
   */
  bool show_config_text(const ParseLineResult& r);

  /**
   * Show manifests. Show a list of manifest if no name is specified
   * Show the details of named manifest if name is specified
   * @param[in] name: name of the manifest
   * @return : void
   */
  bool show_manifest (const ParseLineResult& r);

  /**
   * Show the history of CLI issued till now
   */
  bool show_cli_history(const ParseLineResult& r);

  /**
   * Clear the CLI history
   */
  bool clear_cli_history(const ParseLineResult& r);

  /**
   * Show the CLI transport mode (RW.MSG or NETCONF). 
   */
  bool show_cli_transport(const ParseLineResult& r);

  /**
   * Show the configuration in XML
   */
  bool show_config_xml(const ParseLineResult& r);

  /**
   * Load a configuration from a XML formatted file
   */
  bool load_from_xml(
      const ParseLineResult& r); /**< Name of file from which configuration is to be read */

  /**
   * Load a configuration from a Text formatted file
   */

  bool load_from_config(
      const ParseLineResult& r);/**< Name of file from which configuration is to be read */

  /**
   * Load an xml manifest file to modify the CLI.
   * called as a callback mechanism of fixed parse nodes
   */
  bool load_manifest(
      const ParseLineResult& r);  /**< The parse line that cause the callback */

  bool load_manifest(
      const char *name);  /**< The name of the manifest to be loaded */

  /**
   * Load all the manifest files in the latest version
   * schema directory.
   */
  bool load_manifest_files();


  /**
   * Load a text config file and play it back to setup the system
   * called as a callback mechanism of fixed parse nodes
   */

  bool load_config(
      const ParseLineResult& r);  /**< The parse line that cause the callback */

  /**
   * Load an XML configuration
   */
  bool load_config_xml(
        const ParseLineResult& r);

  /**
   * Print the help associated with a command on the CLI screen
   */
  bool help_command (
      const ParseLineResult& r);  /**< The parse line that cause the callback */

  /**
   * Change the current CLI transport
   */
  bool set_cli_transport(const ParseLineResult& r);

  /**
   * Add builtin commands to the parser. These are not part of the YANG file,
   * since the YANG is used only to specify the schema.
   */
  void add_builtin_cmds();

  // utility functions to functionality
  bool handle_built_in_commands(ParseLineResult* r);

  bool invoke_command(int argc, const char* const* argv);
  bool invoke_command_set(int argc, const char* const* argv);

  /**
   * Set the messaging hook function that will be invoked when messaging
   * is required while executing a command. For use with zsh plugin and
   * might change in future for netconf client.
   */
  void set_messaging_hook(rwcli_messaging_hook hook);

  /**
   * Set the history provider method. This will invoke the zsh plugin to display
   * the history.
   */
  void set_history_hook(rwcli_history_hook hook);

 public:
  // Response print routines handled by the RwCLI

  /**
   * Given a XML string, print it in pretty format
   */
  bool pretty_print_xml(char* xml_str, std::ostream& out);

 public:

  /// Parent CLI, if recursive invocation
  RwCLI* parent_rw_cli;

  /// XML Manager.  Maybe owned, maybe not!
  rw_yang::XMLManager* xmlmgr;

  /// Current configuration.  Maybe owned, maybe not!
  rw_yang::XMLDocument* config;

  RwCliParser *parser;                /**< Parser object associated with this CLI */
  SchemaManager *schema_mgr_;         /**< YANG schema manager for CLI */
  std::ostream &out_stream;           /**< Stream to which output is printed to */
  std::ostream &err_stream;           /**< Stream to which errors are printed to */
  CliManifest *manifest_;             /**< The CLI manifest helper class */

  rwcli_component_ptr_t component;
  rwcli_instance_ptr_t instance;

  rw_vx_framework_t *vxfp;

  bool has_candidate_store_ = false;

  /** Optional, messaging hook which will be used instead of sending it through
   *  RW.MSG
   */
  rwcli_messaging_hook messaging_hook_;

  /**
   * In case zsh based shell is used, retrieve the history from zsh
   */
  rwcli_history_hook history_hook_;

  /**
   * The list of schemas to be exposed.
   */
  std::set<std::string> module_names_;

  rwcli_transport_mode_t transport_mode_ = RWCLI_TRANSPORT_MODE_NETCONF;
  rwcli_user_mode_t user_mode_ = RWCLI_USER_MODE_NONE;

  //
  // Default Pretty Priting Hooks
  //
  std::string default_print_hook_; /**< Default XML to Text hook */ 
  std::string config_print_hook_;  /**< Default config print hook */
  std::string config_print_to_file_hook_; /**< Default config print to file hook */
  std::string pretty_print_xml_hook_; /**< Default XML Pretty print hook */
  std::string pretty_print_xml_to_file_hook_; /**< Default XML Pretty print hook to file*/

  void reset_yang_model();
 private:
  void setup_parser();

};

typedef bool (RwCLI::*RwCliCallback) (const ParseLineResult& r);

class CallbackRwCli:public CliCallback
{
 public:
  RwCliCallback       cb_func_;  /**< The function to be called when execute is called */
  RwCLI               *cb_data_;  /**< Object on which the callback is to be executed */


 public:
  CallbackRwCli(RwCLI& data, RwCliCallback func);
  CallbackRwCli(const CallbackRwCli&) =  delete;                 // not copyable
  CallbackRwCli& operator = (const CallbackRwCli&) = delete;  // not assingnable

  virtual ~CallbackRwCli() {};
  virtual bool execute(const ParseLineResult& r);

};


#endif // defined(__cplusplus) || defined(c_plusplus) || defined (__DOXYGEN__)

#endif //__RWCLI_HPP__
