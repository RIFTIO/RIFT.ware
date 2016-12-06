
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
 * @file rwcli.cpp
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @author Vinod Kamalaraj (vinod.kamalaraj@riftio.com)
 * @date 03/18/2014
 * @brief Rift CLI based on YANG scheme
 */


#include "rwcli_plugin.h"
#include "rwcli_zsh.h"

#include <fstream>
#include <set>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

#include "yangcli.hpp"
#include "yangncx.hpp"
#include "rwcli.hpp"
#include "rwcli.h"
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

using namespace rw_yang;

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

RW_CF_TYPE_DEFINE("RW.Cli RWTasklet Component Type", rwcli_component_ptr_t);
RW_CF_TYPE_DEFINE("RW.Cli RWTasklet Instance Type", rwcli_instance_ptr_t);

static const char* RW_CLI_BASE_MANIFEST = "rw.cli.xml";

/*
 * Generic error handler to supress the generic errors raised by libxml
 */
static void xml_generic_error_handler(void* ctxt, const char* msg, ...)
{
  (void)ctxt;
  (void)msg;
  // Supress the generic errors
}

/**
 * Constructor for the  RwCLI Parser core.
 */
RwCliParser::RwCliParser(YangModel& model, RwCLI& owner)
: BaseCli(model),
  rw_cli(owner)
{
  root_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);
  root_parse_node_->flags_.set_inherit(ParseFlags::MODE_PATH);
  ModeState::ptr_t new_root(new ModeState(*this, *root_parse_node_, 0));
  mode_stack_root(std::move(new_root));
}

RwCliParser::RwCliParser(YangModel& model, RwCLI& owner, ParseFlags::flag_t flag)
: BaseCli(model),
  rw_cli(owner)
{
  root_parse_node_->flags_.set_inherit(ParseFlags::NEXT_TO_SIBLING);
  root_parse_node_->flags_.set_inherit(flag);
  ModeState::ptr_t new_root(new ModeState(*this, *root_parse_node_, 0));
  mode_stack_root(std::move(new_root));
}

bool RwCliParser::handle_command(ParseLineResult* r)
{
  return rw_cli.handle_command(r);
}

void RwCliParser::mode_enter(ParseLineResult* r)
{

  if (r->result_node_->ignore_mode_entry()) {
    // The config behavior node sets mode_entry_ignore.

    // There should be more checks or a better way of doing this
    state_ = BaseCli::STATE_CONFIG;

    
    // Reset flags if only mode paths were allowed
    current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::MODE_PATH);
    current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::CONFIG_ONLY);

    return;
  }

  XMLNode *mode_xml_head = current_mode_->update_xml_cfg(rw_cli.config,
                                                         r->result_tree_);
  RW_ASSERT (mode_xml_head);
  ModeState::ptr_t new_mode(new ModeState (r, mode_xml_head));

  mode_enter_impl(std::move(new_mode));

}

void RwCliParser::mode_enter(ParseNode::ptr_t&& result_tree,
                            ParseNode* result_node)
{
  if (result_node->ignore_mode_entry()) {
    // The config behavior node sets mode_entry_ignore.

    // There should be more checks or a better way of doing this
    state_ = BaseCli::STATE_CONFIG;

    
    // Reset flags if only mode paths were allowed
    current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::MODE_PATH);
    current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::CONFIG_ONLY);

    return;
  }
  

  XMLNode *mode_xml_head =
      current_mode_->update_xml_cfg(rw_cli.config, result_tree);
  
  RW_ASSERT (mode_xml_head);
  ModeState::ptr_t new_mode
      (new ModeState (std::move(result_tree), result_node, mode_xml_head));
  mode_enter_impl(std::move(new_mode));

}

bool RwCliParser::config_behavioral (const ParseLineResult& r)
{
  RW_ASSERT (BaseCli::STATE_CONFIG != state_);

  state_ = BaseCli::STATE_CONFIG;
  // Reset flags if only mode paths were allowed
  current_mode_->top_parse_node_->flags_.clear_inherit(ParseFlags::MODE_PATH);
  current_mode_->top_parse_node_->flags_.set_inherit(ParseFlags::CONFIG_ONLY);
  setprompt();

  // All show commands in config mode must print in config cli format
  show_node_->set_cli_print_hook(rw_cli.config_print_hook_.c_str());
  show_node_->flags_.set(ParseFlags::PRINT_HOOK_STICKY);

  return true;
}

bool RwCliParser::config_exit()
{
  bool can_exit = true;

  // Reset the show node print hook attributes
  show_node_->set_cli_print_hook(nullptr);
  show_node_->flags_.clear(ParseFlags::PRINT_HOOK_STICKY);

  // If there are uncommitted changes, then ask before exiting config mode
  if (rw_cli.has_candidate_store_ && uncommitted_changes_) {
    std::string input;
    if (!auto_commit_) {
      const int buf_size = 256;
      char buf_input[256];
      char* ret = nullptr;
      std::cout << "There are uncommitted changes. Commit the changes (yes/no/CANCEL)? ";
      fflush(stdout);

      // using fgets instead of std::cin because cin doesn't appear to support
      // unbufferred input (raw). Line Editors tend to set the terminal to raw.
      fflush(stdin);
      ret = fgets(buf_input, buf_size, stdin);
      if (ret) {
        int len = strlen(buf_input);
        if (buf_input[len-1] == '\n') {
          // strip the \n at the end
          buf_input[len-1] = '\0';
        }
        input = buf_input;
      } else {
        std::cout << std::endl;
        fflush(stdout);
      }
    } else {
      input = "y";
    }

    if (input == "y" || input == "yes") {
      ParseLineResult r(*this, "commit", ParseLineResult::ENTERKEY_MODE);
      uncommitted_changes_ = false;
      commit_behavioral(r);
    } else if (input == "n" || input == "no") {
      ParseLineResult r(*this, "rollback", ParseLineResult::ENTERKEY_MODE);
      uncommitted_changes_ = false;
      discard_behavioral(r);
    } else {
      can_exit = false;
    }
  }

  return can_exit;
}


XMLDocument::uptr_t RwCliParser::get_command_dom(ParseNode* result_tree)
{
  XMLDocument::uptr_t command = std::move(rw_cli.xmlmgr->create_document());
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

void RwCliParser::associate_default_print_hook()
{
  // The default print hook is assigned on the following nodes
  //  - root node
  //  - no node (within config mode)
  root_parse_node_->set_cli_print_hook(rw_cli.default_print_hook_.c_str());

  // 'no' behavior has a default print hook (cannot be overridden)
  if (no_node_) {
    no_node_->flags_.set(ParseFlags::PRINT_HOOK_STICKY);
    no_node_->set_cli_print_hook(rw_cli.default_print_hook_.c_str());
  }
}

void RwCliParser::associate_config_print_hook()
{
  // The config print hooks should be set on the following execution mode
  // commands
  //  - show config
  //  - show candidate-config
  // For config mode 'show config' and 'show candidate-config', the print-hook
  // will be set on entering the config mode


  if (show_node_) {
    ParseNodeFunctional* fn_node = nullptr;
    fn_node = show_node_->find_descendent("config");
    if (fn_node) {
      fn_node->set_cli_print_hook(rw_cli.config_print_hook_.c_str());
    }
    fn_node = show_node_->find_descendent("candidate-config");
    if (fn_node) {
      fn_node->set_cli_print_hook(rw_cli.config_print_hook_.c_str());
    }
  }
}

void RwCliParser::associate_config_print_to_file_hook()
{
  // save config text
  ParseNodeFunctional* save_config_text = find_descendent_deep(
                                            {"save","config","text"});
  if (save_config_text) {
    save_config_text->set_cli_print_hook(
                        rw_cli.config_print_to_file_hook_.c_str());
  }
}

void RwCliParser::associate_pretty_print_xml_hook()
{
  // show config xml
  if (show_node_) {
    ParseNodeFunctional* fn_node = show_node_->find_descendent_deep(
                                                  {"config", "xml"});
    if (fn_node) {
      fn_node->set_cli_print_hook(rw_cli.pretty_print_xml_hook_.c_str());
    }
  }
}

void RwCliParser::associate_pretty_print_xml_to_file_hook()
{
  // save config xml
  ParseNodeFunctional* save_config_xml = find_descendent_deep(
                                            {"save","config","xml"});
  if (save_config_xml) {
    save_config_xml->set_cli_print_hook(
                        rw_cli.pretty_print_xml_to_file_hook_.c_str());
  }
}

void RwCliParser::enhance_behaviorals()
{
  /* !! * START OF SHOW * !! */
  ParseNodeFunctional *cli_keyword =
      new ParseNodeFunctional (*this, "cli", show_node_.get());
  
  const char *help = "Show CLI information";
  cli_keyword->help_short_set (help);
  cli_keyword->help_full_set (help);

  ParseNodeFunctional *history_keyword =
      new ParseNodeFunctional(*this, "history", cli_keyword);
  
  help = "Show CLI history";
  history_keyword->help_short_set(help);
  history_keyword->help_full_set(help);
  history_keyword->set_callback(new CallbackRwCli
                                (rw_cli, &RwCLI::show_cli_history));

  cli_keyword->add_descendent(history_keyword);

  ParseNodeFunctional *manifest_keyword = new ParseNodeFunctional (*this, "manifest", cli_keyword);
  help = "Show configuration in manifest mode";
  manifest_keyword->help_short_set(help);
  manifest_keyword->help_full_set(help);
  manifest_keyword->set_callback(new CallbackRwCli(rw_cli, &RwCLI::show_manifest));
  cli_keyword->add_descendent (manifest_keyword);
  
  ParseNodeFunctional *transport_keyword =
      new ParseNodeFunctional(*this, "transport", cli_keyword);
  
  help = "Show CLI Transport mode";
  transport_keyword->help_short_set(help);
  transport_keyword->help_full_set(help);
  transport_keyword->set_callback(new CallbackRwCli
                                (rw_cli, &RwCLI::show_cli_transport));

  cli_keyword->add_descendent(transport_keyword);

  show_node_->add_descendent(cli_keyword);

  ParseNodeFunctional *cfg_keyword = new ParseNodeFunctional (*this, "config", show_node_.get());
  help = "Show the configuration";
  cfg_keyword->help_short_set (help);
  cfg_keyword->help_full_set (help);
  cfg_keyword->set_callback(new CallbackRwCli(rw_cli, &RwCLI::show_config_text));
  if (!rw_cli.config_print_hook_.empty()) {
    cfg_keyword->set_cli_print_hook(rw_cli.config_print_hook_.c_str());
  }
  
  ParseNodeFunctional *text_keyword = new ParseNodeFunctional (*this, "text", cfg_keyword);
  help = "Show configuration in text mode";
  text_keyword->help_short_set(help);
  text_keyword->help_full_set(help);

  text_keyword->set_callback(new CallbackRwCli(rw_cli, &RwCLI::show_config_text));

  cfg_keyword->add_descendent(text_keyword);

  ParseNodeFunctional *xml_keyword = new ParseNodeFunctional (*this, "xml", cfg_keyword);
  help = "Show configuration in xml mode";
  xml_keyword->help_short_set(help);
  xml_keyword->help_full_set(help);
  if (!rw_cli.pretty_print_xml_hook_.empty()) {
    xml_keyword->set_cli_print_hook(rw_cli.pretty_print_xml_hook_.c_str());
  }

  xml_keyword->set_callback(new CallbackRwCli(rw_cli, &RwCLI::show_config_xml));

  cfg_keyword->add_descendent (xml_keyword);

  show_node_->add_descendent(cfg_keyword);

  if (has_candidate_store_) {
    ParseNodeFunctional *cand_keyword = new ParseNodeFunctional (*this, 
                                        "candidate-config", show_node_.get());
    help = "Show the candidate configuration";
    cand_keyword->help_short_set (help);
    cand_keyword->help_full_set (help);
    cand_keyword->set_callback(new CallbackBaseCli(this, 
                                    &BaseCli::show_candidate_config));
    if (!rw_cli.config_print_hook_.empty()) {
      cand_keyword->set_cli_print_hook(rw_cli.config_print_hook_.c_str());
    }
    show_node_->add_descendent(cand_keyword);
  }

  // Set the Default print hook on the Parse Root Node. If no print hook is set
  // on the other parse nodes, the default will be applied
  root_parse_node_->set_cli_print_hook(rw_cli.default_print_hook_.c_str());

  // 'no' behavior has a default print hook (cannot be overridden)
  no_node_->flags_.set(ParseFlags::PRINT_HOOK_STICKY);
  no_node_->set_cli_print_hook(rw_cli.default_print_hook_.c_str());
}

bool RwCliParser::show_config(const ParseLineResult& r)
{
  return (rw_cli.process_cli_command (r, nc_get_config));
}

bool RwCliParser::show_candidate_config(const ParseLineResult& r)
{
  return rw_cli.process_cli_command(r, nc_get_candidate_config);
}


bool RwCliParser::show_operational (const ParseLineResult& r)
{
    return (rw_cli.process_cli_command (r, nc_get));
}

bool RwCliParser::process_config (const ParseLineResult& r)
{
  // ATTN: Hack to introduce Role Based Access control for 'cloud' config command
  if (rw_cli.user_mode_ == RWCLI_USER_MODE_OPER && 
      !r.line_words_.empty() &&
      r.line_words_[0] == "cloud") {
    rw_cli.err_stream << "ERROR: Insufficient privilege to execute the command."
                      << std::endl;
    return true;
  }
  return (rw_cli.process_cli_command (r, nc_edit_config));
}

bool RwCliParser::no_behavioral (const ParseLineResult& r)
{
  // ATTN: Hack to introduce Role Based Access control for 'cloud' config command
  if (rw_cli.user_mode_ == RWCLI_USER_MODE_OPER && 
      r.line_words_.size() >= 2 &&
      r.line_words_[1] == "cloud") {
    rw_cli.err_stream << "ERROR: Insufficient privilege to execute the command."
                      << std::endl;
    return true;
  }
  return (rw_cli.process_cli_command (r, nc_edit_config, ec_delete));
}

bool RwCliParser::commit_behavioral (const ParseLineResult& r)
{
  if (rw_cli.has_candidate_store_) {
    return (rw_cli.process_cli_command (r, nc_commit));
  }
  return true;
}

bool RwCliParser::discard_behavioral (const ParseLineResult& r)
{
  if (rw_cli.has_candidate_store_) {
    return (rw_cli.process_cli_command (r, nc_discard_changes));
  }
  return true;
}

void RwCliParser::handle_parse_error (const parse_result_t result,
                                      const std::string line)
{
  switch (result) {
    case PARSE_LINE_RESULT_SUCCESS:
      RW_CRASH();
      break;
    case PARSE_LINE_RESULT_INVALID_INPUT:
      rw_cli.err_stream
          << "ERROR: Invalid command or parameter values:"
          <<line << std::endl;
      
      break;
    case PARSE_LINE_RESULT_NO_COMPLETIONS:
      rw_cli.err_stream
          << "ERROR: Input does not match any commands or parameters for "
          << line << std::endl;
      break;
    case PARSE_LINE_RESULT_AMBIGUOUS:
      rw_cli.err_stream
          << "Command entered is ambiguous :"<< line << std::endl;
      break;
    case PARSE_LINE_RESULT_INCOMPLETE:
      rw_cli.err_stream
          << "Command is incomplete, or parameters missing:" << line
          << std::endl;
      break;
    case PARSE_LINE_RESULT_MISSING_MANDATORY:
      rw_cli.err_stream
          << "ERROR: Missing mandatory fields: " << line
          << std::endl << std::endl;
      break;
    default:
      RW_CRASH();
  }
         
}
/*****************************************************************************/
/****                      CLI   MANIFEST                               ******/
/*****************************************************************************/

CliManifest::CliManifest(RwCLI &rw_cli_in)
    :rw_cli(rw_cli_in)
{
}

rw_status_t CliManifest::show()
{
  rw_status_t status = RW_STATUS_SUCCESS;
  size_t i = 0;

  if (mfiles.size()) {
    std::cout << "\t Manifest Files:" << std::endl;
    for (const auto& mfile : mfiles) {
      std::cout << "\t\t" << mfile << std::endl;
    }
  }

  if (rw_cli.schema_mgr_->loaded_schema_.size()) {
    std::cout << "\tYANG Modules:" << std::endl;
    rw_cli.schema_mgr_->show_schemas(std::cout);
  }

  if (namespaces.size()) {
    std::cout << "\tNamespaces:" << std::endl;
    for (i = 0; i < namespaces.size(); ++i) {
      std::cout << "\t\t" << namespaces[i] << std::endl;
    }
  }

  if (plugin_iface_map.size()) {
    std::cout << "\tPlugin list:" << std::endl;
    std::map<std::string, void *>::iterator ii;
    for(ii = plugin_iface_map.begin();
        ii != plugin_iface_map.end(); ++ii) {
      std::cout << "\t\t" << (*ii).first;
      std::cout << ": " << plugin_repository_map[(*ii).first.c_str()];
      if ((*ii).second == nullptr) {
        std::cout << "** NOT LOADED";
      }
      std::cout << std::endl;
    }
  }

  if (cli_modes.size()) {
    std::map<std::string, std::string>::iterator it;

    std::cout << "\tCLI modes:" << std::endl;
    for (it = cli_modes.begin(); it != cli_modes.end(); it++) {
      std::cout << "\t\t" << it->first << ":" << it->second << std::endl;
    }
  }

  if (cli_print_hooks.size()) {
    std::map<std::string, std::string>::iterator it;

    std::cout << "\tCLI Print Hooks:" << std::endl;
    for (it = cli_print_hooks.begin(); it != cli_print_hooks.end(); it++) {
      std::cout << "\t\t" << it->first << ":" << it->second << std::endl;
    }
  }

  return status;
}

rw_status_t CliManifest::add_yang_module(char *module_name)
{
  return rw_cli.schema_mgr_->load_schema(module_name);
}

CliManifest::ReadingStatus CliManifest::add_cli_print_hook(const char *path, const char *hook)
{
  RW_ASSERT(path);
  RW_ASSERT(hook);

  // Make a new base cli that will skip keys in completions
  RwCliParser tmp_parser(*(rw_cli.schema_mgr_->model_), rw_cli,
                         ParseFlags::V_LIST_KEYS_CLONED);

  std::string spaced_path = path;
  spaced_path += ' ';
  // Find a path to this node in the yang module - spoof a cli command
  ParseLineResult r(tmp_parser, spaced_path.c_str(), ParseLineResult::ENTERKEY_MODE);

  if (!r.success_) {
    return ReadingStatus(RW_STATUS_FAILURE, r.error_);
  }

  cli_print_hooks[path] = hook;

  r.result_node_->set_cli_print_hook(cli_print_hooks[path].c_str());
  return ReadingStatus(RW_STATUS_FAILURE, r.error_);
}

CliManifest::ReadingStatus CliManifest::add_cli_mode(const char *mode, const char *display)
{
  // Make a new base cli that will skip keys in completions
  RwCliParser tmp_parser(*(rw_cli.schema_mgr_->model_), rw_cli, 
                         ParseFlags::V_LIST_KEYS_CLONED);

  // Find a path to this node in the yang module - spoof a cli command
  ParseLineResult r(tmp_parser, mode, ParseLineResult::ENTERKEY_MODE);

  if (!r.success_) {
    return RW_STATUS_FAILURE;
  }

  if (r.result_node_->is_leafy()) {
    return RW_STATUS_FAILURE;
  }

  if (nullptr == display) {
    display = r.result_node_->value_.c_str();
  }

  RW_ASSERT(display);

  cli_modes[mode] = display;
  r.result_node_->set_mode (cli_modes[mode].c_str());
  return RW_STATUS_SUCCESS;
}


rw_status_t CliManifest::add_plugin(char *plugin,
                                    char *typelib)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  std::string plugin_local(plugin);

  /// Add the plugin to the map of plug ins

  plugin_repository_map[plugin_local] = std::string(typelib);

  // PeasExtension *peas_extension = nullptr;
  rw_vx_modinst_common_t *common = nullptr;

  char * name_version = strdup(typelib);
  char * version = strchr(name_version, '/');
  *version = '\0';
  version++;
  rw_vx_require_repository(name_version, version);
  free(name_version);

  /// Register the plugin
  status = rw_vx_library_open(rw_cli.vxfp,
                              (char *)plugin_local.c_str(),
                              (char *)"",
                              &common);

  RW_ASSERT((RW_STATUS_SUCCESS == status) && (nullptr != common));

  /// Add the plugin to the extensions map
  rwcli_pluginPrint *klass; 
  rwcli_pluginPrintIface *iface;

  // Get the rwtasklet api and interface pointers
  status = rw_vx_library_get_api_and_iface(common,
                                           RWCLI_PLUGIN_TYPE_PRINT,
                                           (void **) &klass,
                                           (void **) &iface,
                                           nullptr);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  RW_ASSERT(klass);
  RW_ASSERT(iface);

  plugin_iface_map[plugin_local] = (void *)iface;
  plugin_klass_map[plugin_local] = (void *)klass;
  return status;
}

rw_status_t CliManifest::read_cli_manifest(const char* afpath)
{
  xmlDoc *doc;
  rw_status_t status = RW_STATUS_SUCCESS;

  xmlSetGenericErrorFunc(NULL, xml_generic_error_handler);

  std::string name(afpath);
  if (mfiles.find(name) != mfiles.end()) {
    return RW_STATUS_SUCCESS;
  }

  doc = xmlReadFile(name.c_str(), nullptr, 0);
  if (doc == nullptr) {
    return RW_STATUS_FAILURE;
  }

  mfiles.insert(name);

  status = read_cli_manifest_common(doc);

  xmlFreeDoc(doc);

  return status;
}

rw_status_t CliManifest::read_base_cli_manifest(const char* file)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  xmlDoc *doc;

  if (file == nullptr) {
    return RW_STATUS_SUCCESS;
  }

  if (mfiles.find(file) != mfiles.end()) {
    // Already loaded
    return RW_STATUS_SUCCESS;
  }

  xmlSetGenericErrorFunc(NULL, xml_generic_error_handler);

  doc = xmlReadFile(file, nullptr, 0);
  if (doc == nullptr) {
    // It the file doesn't exists it will be retried later
    return RW_STATUS_FAILURE;
  }

  mfiles.insert(file);
  
  status = read_cli_manifest_common(doc);
  if (status != RW_STATUS_SUCCESS) {
    xmlFreeDoc(doc);
    return status;
  }

  status = read_cli_manifest_defaults(doc);
  xmlFreeDoc(doc);

  return status;
}

rw_status_t CliManifest::read_cli_manifest_common(
                  xmlDoc *doc)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  xmlXPathContextPtr context;
  xmlChar *xpath_query;
  xmlXPathObjectPtr result;
  xmlNodeSetPtr nodeset;
  ReadingStatus reading_status(RW_STATUS_SUCCESS);

  /**
   * Read the plugin list from xml specification.
   */
  xpath_query = (xmlChar *)"/cli/plugin-list/plugin";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;

  for (int i = 0; i < nodeset->nodeNr; ++i) {

    xmlNode *node = nodeset->nodeTab[i];
    char *plugin = (char *)xmlGetProp(node, (xmlChar *) "name");
    char *typelib = (char *)xmlGetProp(node, (xmlChar *) "typelib");

    status = add_plugin(plugin, typelib);

    if (status != RW_STATUS_SUCCESS) {
      rw_cli.err_stream << "Error: failed to plugin: "
                        << std::string(plugin) << std::endl;
    }
    xmlFree(plugin);
    xmlFree(typelib);
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  /*
   * Read the yang files from xml specification
   * and parse them
   */
  
  xpath_query = (xmlChar *) "/cli/yang-modules/module";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;

  for (int i = 0; i < nodeset->nodeNr; ++i) {

    xmlNode *node = nodeset->nodeTab[i];
    char *module = (char *)xmlGetProp(node, (xmlChar *) "name");

    if (rw_cli.module_names_.find(module) == rw_cli.module_names_.end()) {
      continue;
    }

    status = add_yang_module(module);

    if (status != RW_STATUS_SUCCESS) {
      rw_cli.err_stream << "Error: failed to add yang module: "
                        << std::string(module) << std::endl;
    }
    xmlFree(module);
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  xpath_query = (xmlChar *) "/cli/namespace-list/namespace";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;

  for (int i = 0; i < nodeset->nodeNr; ++i) {

    /* ATTN There is no reason to specify a name space in
       the CLI manifest. log a error if any are specified */
    
    xmlNode *node = nodeset->nodeTab[i];
    char *ns = (char *)xmlGetProp(node, (xmlChar *) "name");

    if (status != RW_STATUS_SUCCESS) {
      rw_cli.err_stream << "Error: failed to add namespace: "
                        << std::string(ns) << std::endl;
    }
    xmlFree(ns);
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  /*
   * Read the cli modes and apply them
   */
  xpath_query = (xmlChar *) "/cli/modifications/addMode";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;

  for (int i = 0; i < nodeset->nodeNr; i++) {

    xmlNode *node = nodeset->nodeTab[i];
    char *mode = (char *)xmlGetProp(node, (xmlChar *) "src");
    char *display = (char *) xmlGetProp(node, (xmlChar *) "display");

    reading_status = add_cli_mode(mode, display);

    if (status != RW_STATUS_SUCCESS) {
      rw_cli.err_stream << "Error: failed to add cli mode: "
                        << std::string(mode)
                        << " " << reading_status.message
                        << std::endl;
    }
    xmlFree(mode);
    xmlFree(display);
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);


  /*
   * Read the cli print hooks and apply them
   */
  xpath_query = (xmlChar *) "/cli/modifications/addPrintHook";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;

  for (int i = 0; i < nodeset->nodeNr; i++) {

    xmlNode *node = nodeset->nodeTab[i];
    char *path = (char *)xmlGetProp(node, (xmlChar *) "path");
    char *hook = (char *) xmlGetProp(node, (xmlChar *) "hook");

    reading_status = add_cli_print_hook(path, hook);

    if (status != RW_STATUS_SUCCESS) {
      rw_cli.err_stream << "Error: failed to add cli hook: "
                        << std::string(path) << "[" 
                        << std::string(hook) << "]"
                        << reading_status.message << std::endl;
    }
    xmlFree(path);
    xmlFree(hook);
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  /**
   * Read the to be suppressed path or namespace
   */
  xpath_query = (xmlChar *) "/cli/modifications/suppress";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;

  for (int i = 0; i < nodeset->nodeNr; i++) {

    xmlNode *node = nodeset->nodeTab[i];
    char *ns = (char *)xmlGetProp(node, (xmlChar *) "namespace");
    char *path = (char *) xmlGetProp(node, (xmlChar *) "path");

    if (ns && path) {
      rw_cli.err_stream << "Error: Cannot suppress both Namespace and Path - "
                        << " Namespace: " << ns
                        << " Path: " << path << std::endl;
      xmlFree(ns);
      xmlFree(path);
      return RW_STATUS_FAILURE;
    }

    if (ns) {
      bool success = rw_cli.parser->suppress_namespace(ns);
      if (!success) {
        rw_cli.err_stream << "Error: Failed to suppress namespace - "
                          << ns << std::endl;
      }
    }
    if (path) {
      // Make a new base cli that will skip keys in completions
      RwCliParser tmp_parser(*(rw_cli.schema_mgr_->model_), rw_cli,
                         ParseFlags::V_LIST_KEYS_CLONED);

      bool success = tmp_parser.suppress_command_path(path);
      if (!success) {
        rw_cli.err_stream << "Error: Failed to suppress command path - "
                          << ns << std::endl;
      }
    }

    xmlFree(ns);
    xmlFree(path);
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  return status;
}

rw_status_t CliManifest::read_cli_manifest_defaults(
                  xmlDoc *doc)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  xmlXPathContextPtr context;
  xmlChar *xpath_query;
  xmlXPathObjectPtr result;
  xmlNodeSetPtr nodeset;

  // Populate the defaults
  xpath_query = (xmlChar *) "/cli/defaults/defaultPrintHook";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;
  if (nodeset->nodeNr) {
    xmlNode *node = nodeset->nodeTab[0];
    char* prop = (char*)xmlGetProp(node, (xmlChar*)"hook");
    rw_cli.default_print_hook_ = prop;
    xmlFree(prop);
    
    if (rw_cli.parser) {
      rw_cli.parser->associate_default_print_hook();
    }
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  xpath_query = (xmlChar *) "/cli/defaults/configPrintHook";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;
  if (nodeset->nodeNr) {
    xmlNode *node = nodeset->nodeTab[0];
    char* prop = (char*)xmlGetProp(node, (xmlChar*)"hook");
    rw_cli.config_print_hook_ = prop;
    xmlFree(prop);

    if (rw_cli.parser) {
      rw_cli.parser->associate_config_print_hook();
    }
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  xpath_query = (xmlChar *) "/cli/defaults/configPrintToFileHook";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;
  if (nodeset->nodeNr) {
    xmlNode *node = nodeset->nodeTab[0];
    char* prop = (char*)xmlGetProp(node, (xmlChar*)"hook");
    rw_cli.config_print_to_file_hook_ = prop;
    xmlFree(prop);

    if (rw_cli.parser) {
      rw_cli.parser->associate_config_print_to_file_hook();
    }
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  xpath_query = (xmlChar *) "/cli/defaults/prettyPrintXmlHook";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;
  if (nodeset->nodeNr) {
    xmlNode *node = nodeset->nodeTab[0];
    char* prop = (char*)xmlGetProp(node, (xmlChar*)"hook");
    rw_cli.pretty_print_xml_hook_ = prop;
    xmlFree(prop);

    if (rw_cli.parser) {
      rw_cli.parser->associate_pretty_print_xml_hook();
    }
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  xpath_query = (xmlChar *) "/cli/defaults/prettyPrintXmlToFileHook";
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression(xpath_query, context);
  nodeset = result->nodesetval;
  if (nodeset->nodeNr) {
    xmlNode *node = nodeset->nodeTab[0];
    char* prop = (char*)xmlGetProp(node, (xmlChar*)"hook");
    rw_cli.pretty_print_xml_to_file_hook_ = prop;
    xmlFree(prop);

    if (rw_cli.parser) {
      rw_cli.parser->associate_pretty_print_xml_to_file_hook();
    }
  }
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);

  return status;
}

/**
 * Create a RwCLI object.  Creates the syntax model, loads the basic
 * RW.CLI syntax, initializes the CLI parser, and creates an empty
 * configuration.
 */
RwCLI::RwCLI(rwcli_transport_mode_t transport_mode,
        rwcli_user_mode_t user_mode)
: parent_rw_cli(nullptr),
  xmlmgr(nullptr),
  config(nullptr),
  parser(nullptr),
  schema_mgr_(nullptr),
  out_stream(std::cout),
  err_stream(std::cerr),
  messaging_hook_(nullptr),
  transport_mode_(transport_mode),
  user_mode_(user_mode)
{
  // Create the syntax model. The other code assumes this exists.
  schema_mgr_ = new SchemaManager();

  // Create the cli manifest helper instance.
  manifest_ = new CliManifest(*this);
}

void RwCLI::setup_parser()
{
  if (parser){
    delete parser;
  }

  (void)schema_mgr_->load_all_schemas();

  // Create a RwCliParser instance
  parser = new RwCliParser( *(schema_mgr_->model_), *this );
  RW_ASSERT(parser);
  parser->has_candidate_store_ = has_candidate_store_;

  // Create the empty config
  xmlmgr = xml_manager_create_xerces(schema_mgr_->model_.get()).release();
  RW_ASSERT(xmlmgr);
  config = xmlmgr->create_document().release();
  RW_ASSERT(config);
  parser->root_mode_->set_xml_node(config->get_root_node());
}

void RwCLI::reset_yang_model()
{
  (void)schema_mgr_->load_all_schemas();
  (void)load_manifest_files();
}

/**
 * Create a child RwCLI object, which begins parsing from the root
 * context.  It uses the same syntax model and config as the
 * parent object.
 */
RwCLI::RwCLI(RwCLI* other)
: parent_rw_cli(other),
  xmlmgr(other->xmlmgr),
  config(other->config),
  parser(nullptr),
  schema_mgr_(other->schema_mgr_),
  out_stream(std::cout),
  err_stream(std::cerr),
  messaging_hook_(other->messaging_hook_),
  transport_mode_(other->transport_mode_),
  user_mode_(other->user_mode_),
  default_print_hook_(other->default_print_hook_),
  config_print_hook_(other->config_print_hook_),
  config_print_to_file_hook_(other->config_print_to_file_hook_),
  pretty_print_xml_hook_(other->pretty_print_xml_hook_),
  pretty_print_xml_to_file_hook_(other->pretty_print_xml_to_file_hook_)
{
  RW_ASSERT(other);
  RW_ASSERT(other->schema_mgr_);
  RW_ASSERT(other->config);
  RW_ASSERT(other->parser);
  RW_ASSERT(other->parser->root_mode_);

  // Create a RwCliParser instance
  parser = new RwCliParser( *(schema_mgr_->model_), *this );
  RW_ASSERT(parser);
  parser->add_builtins();
  parser->add_behaviorals();
  add_builtin_cmds();
  parser->setprompt();
}

/**
 * Destroy RwCLI object and all related objects.
 */
RwCLI::~RwCLI()
{
  if (parent_rw_cli) {
    // Don't free anything owned by the parent
    if (parent_rw_cli->config == config) {
      config = nullptr;
    }
    if (parent_rw_cli->xmlmgr == xmlmgr) {
      xmlmgr = nullptr;
    }
    if (parent_rw_cli->schema_mgr_ == schema_mgr_) {
      schema_mgr_ = nullptr;
    }
    // ATTN: Transfer status and context?
  }

  if (parser) {
    delete parser;
  }
  if (config) {
    config->obj_release();
  }
  if (xmlmgr) {
    xmlmgr->obj_release();
  }
  if (schema_mgr_) {
    delete schema_mgr_;
  }
}

void RwCLI::initialize(const char* base_manifest_file)
{
  // The plugin framework must be initialized before this, since the base cli
  // manifest might load plugins
  manifest_->read_base_cli_manifest(base_manifest_file);

  setup_parser();
  
  // The builtin and behavioral nodes might require default parameters set in
  // the base cli manifest.
  parser->add_builtins();
  parser->setprompt();
  parser->add_behaviorals();
  add_builtin_cmds();
}

bool RwCLI::load_from_xml(const ParseLineResult& r)
{
  RW_BCLI_MIN_ARGC_CHECK(&r,5);
  
  const char *filename = r.line_words_[4].c_str();
  RW_ASSERT(filename);

  XMLDocument::uptr_t new_dom = std::move(xmlmgr->create_document_from_file(filename, true/*validate*/));
  if (!new_dom || !new_dom->get_root_node()) {
    // ATTN: complain about the error!
    return false;
  }

  // Try to splice the new document into the current configuration.
  // ATTN: Would be nice if the location of the loaded file could be
  //       relative to the current mode stack.  In order to do that,
  //       the load commands would have to be inserted into the model
  //       at "namespace-change" locations, I think (anything other
  //       than that would be harder to accomplish).
  RW_ASSERT(config);
  rw_yang_netconf_op_status_t ncs = config->merge(new_dom.get());
  // ATTN: Should complain about errors!

  // ATTN: new_loc probably goes into the mode stack...
  return (RW_YANG_NETCONF_OP_STATUS_OK == ncs);
}

bool RwCLI::save_config(const ParseLineResult& r)
{
  clock_t begin = clock();

  RW_BCLI_MIN_ARGC_CHECK(&r,4);

  const char* config_str = "<?xml version= \"1.0\" encoding=\"UTF-8\"?> \n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\"/>";
  NetconfRsp *rsp = nullptr;

  rw_status_t rs = process_cli_command((char*)config_str, &rsp, nc_get_config);
  if (rs != RW_STATUS_SUCCESS) {
    std::cout << "Error: Fetch config failed.\n";
    return false;
  }
  if (rsp == nullptr) {
    std::cout << "No response received\n";
    return false;
  }
  print_hook(r, rsp);

  clock_t end = clock();
  double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
  
  std::cout << "Save config successful" << std::endl;
  std::cout << "Elapsed time: " << elapsed_secs << " sec" << std::endl;

  return true;
}

bool RwCLI::show_config_text(const ParseLineResult&r)
{
  process_cli_command(r, nc_get_config);
  return true;
}

bool RwCLI::show_config_xml(const ParseLineResult& r)
{
  process_cli_command(r, nc_get_config);
  return true;
}


bool RwCLI::show_manifest(const ParseLineResult& r) 
{
  manifest_->show();
  return true;
}

bool RwCLI::show_cli_history(const ParseLineResult& r) {
  UNUSED (r);

  // ZSH stores the history for RW.CLI
  if (history_hook_) {
    history_hook_();
  }
  return true;
}

bool RwCLI::show_cli_transport(const ParseLineResult& r) 
{
  RW_ASSERT(messaging_hook_);
  messaging_hook_(RWCLI_MSG_TYPE_GET_TRANSPORT_STATUS, nullptr, nullptr);

  return true;
}

/**
  * Show available help
 */
bool RwCLI::help_command (const ParseLineResult& r)
{
  RW_BCLI_MIN_ARGC_CHECK(&r,3);
  const char *name = r.line_words_[2].c_str();
  rw_yang_node_t*node =  rw_yang_find_node_in_model(schema_mgr_->model_.get(),
                                                    name);
  
  if (nullptr != node){
    rw_yang_node_dump_tree(node, 2, 1);
  }else{
    rw_yang_model_dump(schema_mgr_->model_.get(), 2/*indent*/, 1/*verbosity*/);
  }

  return true;
}
bool RwCLI::clear_cli_history(const ParseLineResult& r) {
  UNUSED (r);
  return true;
}

bool RwCLI::set_cli_transport(const ParseLineResult& r)
{
  RW_ASSERT(messaging_hook_);
  RW_BCLI_MIN_ARGC_CHECK(&r,3);
  rwcli_internal_req_t req;
  
  if (r.line_words_[2] == "netconf") {
    req.u.transport_mode = RWCLI_TRANSPORT_MODE_NETCONF;
  } else if (r.line_words_[2] == "rwmsg") {
    req.u.transport_mode = RWCLI_TRANSPORT_MODE_RWMSG;
  } else {
    err_stream << "ERROR: Invalid transport type - " << r.line_words_[2] << std::endl;
    return false;
  }

  messaging_hook_(RWCLI_MSG_TYPE_SET_CLI_TRANPORT, &req, nullptr);

  return true;
}

/**
 * Invoke a single command specified in an argument vector.
 * Does NOT support changing modes!
 * @return true if successful, otherwise false.
 */
bool RwCLI::invoke_command(
  int argc, ///< [in] The number of arguments in argv. May be 0
  const char* const* argv ///< [in] The argument vector. May be nullptr is argc==0.
)
{
  RW_ASSERT(0 == argc || argv);
  if (0 == argc) {
    return true;
  }

  RW_ASSERT(parser);
  return parser->parse_argv_one(argc, argv);
}

/**
 * Invoke a series of command lines, specified in an argument vector.
 * @return true if successful, otherwise false.
 */
bool RwCLI::invoke_command_set(
  int argc, ///< [in] The number of lines in argv. May be 0
  const char* const* argv ///< [in] The line vector. May be nullptr is argc==0.
)
{
  RW_ASSERT(0 == argc || argv);
  if (0 == argc) {
    return true;
  }

  RW_ASSERT(parser);
  return parser->parse_argv_set(argc, argv);
}

/*
 * ATTN: this code looks dumb.  Shouldn't this be auto-generated?
 * This code is just a duplication of the yang syntax tree...  There
 * ought to be yang annotations that indicate which function to call.
 * Something like:
 *
 *   rift:cli-function "function_name"
 *
 */
bool RwCLI::handle_built_in_commands(ParseLineResult* r){
  // Look for builtin commands

  if (r->line_words_.size() > 0) {

    if (r->line_words_[0] == "exit" ) {
      RW_BCLI_ARGC_CHECK(r,1);
      RW_ASSERT(parser);
      parser->mode_exit();
      return true;
    }

    if (r->line_words_[0] == "end" ) {
      RW_BCLI_ARGC_CHECK(r,1);
      RW_ASSERT(parser);
      parser->mode_end_even_if_root();
      return true;
    }

    if (r->line_words_[0] == "quit" ) {
      RW_BCLI_ARGC_CHECK(r,1);
      RW_CRASH();
      return true;
    }

    if (r->line_words_[0] == "load" ) {
      RW_BCLI_MIN_ARGC_CHECK(r,2);

      if (r->line_words_[1] == "cli" ) {
        RW_BCLI_MIN_ARGC_CHECK(r,3);

        if (r->line_words_[2] == "manifest" ) {
          load_manifest (*r);
          //RW_BCLI_RW_STATUS_CHECK(rs);

        }
      }
      else if (r->line_words_[1] == "config") {
        RW_BCLI_MIN_ARGC_CHECK(r,3);

        if (r->line_words_[2] == "filename" ) {
          load_config (*r);
          //RW_BCLI_RW_STATUS_CHECK(rs);

        }
      }
      return true;
    }

    if (r->line_words_[0] == "help" ) {
      RW_BCLI_MIN_ARGC_CHECK(r,1);
      if (r->line_words_[1] == "command" ) {
        RW_BCLI_MIN_ARGC_CHECK(r,3);
        rw_yang_node_t*node =  rw_yang_find_node_in_model(
                                    schema_mgr_->model_.get(),
                                    r->line_words_[2].c_str());
        if (node)
          rw_yang_node_dump_tree(node, 2, 1);
      }else{
        rw_yang_model_dump(schema_mgr_->model_.get(), 2/*indent*/, 1/*verbosity*/);
      }
      return true;
    }

    // SAVE commands

    if (r->line_words_[0] == "save") {
      RW_BCLI_MIN_ARGC_CHECK(r,3);
      if (r->line_words_[1] == "config" && r->line_words_[2] == "text") {
        RW_BCLI_MIN_ARGC_CHECK(r,5);
        if (!config_print_to_file_hook_.empty()) {
          r->cli_print_hook_string_ = config_print_to_file_hook_.c_str();
        }
        save_config(*r);
        return true;
      }

      if (r->line_words_[1] == "config" && r->line_words_[2] == "fooxml") {
        RW_BCLI_MIN_ARGC_CHECK(r,5);
        if (!pretty_print_xml_hook_.empty()) {
          r->cli_print_hook_string_ = pretty_print_xml_hook_.c_str();
        }
        save_config(*r);
        return true;
      }
    }


    if (r->line_words_[0] == "show") {
      RW_BCLI_MIN_ARGC_CHECK(r,3);

      if (r->line_words_[1] == "config") {
        return false;
      }

      if (r->line_words_[1] == "cli") {
        if  (r->line_words_[2] == "history") {
          show_cli_history(*r);
          return true;
        }

        if (r->line_words_[2] == "manifest") {
          RW_BCLI_MIN_ARGC_CHECK(r,4);
          if (r->line_words_[3] == "list-name") {
            show_manifest(*r);
            return true;
          }

          if (r->line_words_[3] == "name") {
            RW_BCLI_MIN_ARGC_CHECK(r,5);
            show_manifest(*r);
            return true;
          }
          return false;
        }
        return false;
      }
      return false;
    }

    if ("clear" == r->line_words_[0]) {
      RW_BCLI_MIN_ARGC_CHECK(r,3);
      if (r->line_words_[1] == "cli") {
        if  (r->line_words_[2] == "history") {
          clear_cli_history(*r);
          return true;
        }
      }
    }

  }
  return false;
}

bool RwCLI::is_error_response(const char* xml_str)
{
  bool is_error = false;
  int xml_size = strlen(xml_str);
  xmlDoc *doc = xmlReadMemory(xml_str, xml_size, nullptr, nullptr, 0);

	xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(context,  (xmlChar*) "nc", 
                     (xmlChar*)"urn:ietf:params:xml:ns:netconf:base:1.0");
	xmlXPathObjectPtr result = xmlXPathEvalExpression((xmlChar*)"//nc:rpc-error", 
                                                    context);

	if(!xmlXPathNodeSetIsEmpty(result->nodesetval)){
    is_error = true;
  } 	
  
  xmlXPathFreeObject(result);
  xmlFreeDoc(doc);

  return is_error;
}

bool RwCLI::process_rpc_response(const ParseLineResult& r,
                          NetconfOperations op,
                          NetconfEditConfigOperations ec_op,
                          NetconfRsp *rsp)
{
  bool is_error = false;
  
  if (rsp->xml_response && rsp->xml_response->xml_blob) {
    is_error = is_error_response(rsp->xml_response->xml_blob);
  }

  if (!is_error) {
    if (has_candidate_store_) {
      if (op == nc_edit_config) {
        parser->uncommitted_changes_ = true;
      } else if (op == nc_discard_changes || 
                 op == nc_commit) {
        parser->uncommitted_changes_ = false;
      }
    }
  }

  // A successful response for the command is obtained
  // display the response.
  // if a print hook exists call the hook function
  int print_done = 0;
  if (r.cli_print_hook_string_) {
    rw_status_t s = print_hook(r, rsp);
    if (s == RW_STATUS_SUCCESS) {
      print_done = 1;
    }
  }

  if (!print_done) {
    // either there was no print hook or the hook failed to output the message
    if (rsp->xml_response && rsp->xml_response->xml_blob && '\0' != rsp->xml_response->xml_blob[0]) {
      printf("%s\n", rsp->xml_response->xml_blob);
    } else {
      printf("No XML data!\n");
    }
  }
  return !is_error;
}

bool RwCLI::process_cli_command (const ParseLineResult& r,
                                 NetconfOperations op,
                                 NetconfEditConfigOperations ec)
{
  ParseNode *pn = r.result_tree_.get();
  RW_ASSERT(pn);

  XMLDocument::uptr_t cmd_dom = std::move(parser->get_command_dom(pn));
  XMLNode* root = cmd_dom->get_root_node();
  RW_ASSERT(root);

  /* NOTE:
   * if you need to get the whole config in xml
   * std::string cfg_str =  domoutput::dom_to_str(config, "config");
   */
  std::string command_str("<?xml version= \"1.0\" encoding=\"UTF-8\"?> \n");

  // Commit operation doesn't need an XML 
  if (op != nc_commit && op != nc_discard_changes) {
    command_str += root->to_string();
  }

  /*
   * Initiate a netconf/management request
   * If the cli_print_hook is present, call the
   * corresponding plugin to show the output.
   */

  // invoke the command and get the response
  NetconfRsp *rsp = nullptr;
  rw_status_t s = process_cli_command((char *)command_str.c_str(), &rsp, op, ec);

  if (s == RW_STATUS_SUCCESS) {
    process_rpc_response(r, op, ec, rsp);
  }
  return true;
}
rw_status_t RwCLI::process_cli_command(char *command,
                                       NetconfRsp **rsp,
                                       NetconfOperations op,
                                       NetconfEditConfigOperations ec)
{
  rw_status_t rwstatus;

  // Invoke the hardcoded command
  XmlBlob blob /*= XML_BLOB__INIT*/;
  xml_blob__init(&blob);
  blob.xml_blob = command;

  NetconfReq req;
  netconf_req__init (&req);

  if (command) {
    req.xml_blob = &blob;
  }
  
  req.has_operation = 1;
  req.operation = op;

  // For Netconf mode ec_delete is not used
  // In RW.NSG mode with XML-Agent, ec_delete is not required
  // XML-Agent during copy-merge checks the operation=delete
  if (nc_edit_config == op) {
    req.has_edit_config_operation = 1;
    req.edit_config_operation = ec_merge;
  }

  if (messaging_hook_) {
    return messaging_hook_(RWCLI_MSG_TYPE_RW_NETCONF, (void*)&req, (void**)rsp);
  }

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);

  rwmsg_request_t *rwreq = nullptr;
  rwstatus = rw_uagent__netconf_request_b(&instance->uagent.rwuacli, 
                                          instance->uagent.dest, &req, &rwreq);
  if (RW_STATUS_SUCCESS == rwstatus) {
    //  get the response structure
    rwstatus  = rwmsg_request_get_response(rwreq, (const void**)rsp);
  } else {
    printf("Error %d!\n", rwstatus);
  }

  return rwstatus;
}

bool RwCLI::print_hook_self(const ParseLineResult &r,
                              NetconfRsp *rsp,
                              const std::string& routine)
{
  if (rsp->xml_response && rsp->xml_response->xml_blob && '\0' != rsp->xml_response->xml_blob[0]) {
    if (routine == "pretty_print_xml") {
      pretty_print_xml(rsp->xml_response->xml_blob, std::cout);
    } else if (routine == "pretty_print_xml_file") {
      uint32_t last_word = r.line_words_.size() - 1;
      const char *filename = r.line_words_[last_word].c_str();
      std::ofstream out_file(filename);

      if (out_file.fail()) {
        std::cerr << "File open to write XML failed." << std::endl;
        return false;
      }
      pretty_print_xml(rsp->xml_response->xml_blob, out_file);
    }
  }
  return true;
}

rw_status_t RwCLI::print_hook(const ParseLineResult &r,
                              NetconfRsp *rsp)
{
  rw_status_t status = RW_STATUS_FAILURE;
  rwcli_pluginPrint *klass = nullptr;
  rwcli_pluginPrintIface *iface = nullptr;
  const char *plugin_name;
  // const char *function_name;
  std::vector<std::string> words;
  std::string plugin = std::string(r.cli_print_hook_string_);

  if (plugin.empty()) {
    plugin = default_print_hook_;
  }

  /* hook is specified with a colon seperating the pluing and api
   * For example, rwcli_plugin:default_print */
  split_line(plugin, ':', words);
  if (words.size() != 2) {
    std::cout << "Invalid cli print hook " << r.cli_print_hook_string_ << std::endl;
    return RW_STATUS_FAILURE;
  }
  plugin_name = words[0].c_str();

  if (plugin_name[0] == '.') {
    print_hook_self(r, rsp, words[1]);
    return RW_STATUS_SUCCESS;
  }
  
  /* search for the plugin interface and class */
  iface = (rwcli_pluginPrintIface *)manifest_->plugin_iface_map[plugin_name];
  klass = (rwcli_pluginPrint *)manifest_->plugin_klass_map[plugin_name];

  /* now invoke the api, if plugin interface and class are found */
  if (iface && klass) {
    if (rsp->xml_response && rsp->xml_response->xml_blob && '\0' != rsp->xml_response->xml_blob[0]) {
      if (words[1]=="default_print") {
        iface->default_print(klass, rsp->xml_response->xml_blob);
      } else if (words[1] == "config_writer") {
        iface->print_using_model(klass,
                schema_mgr_->model_.get(),
                rsp->xml_response->xml_blob,
                words[1].c_str(),
                nullptr); // Print to stdout
      } else if (words[1] == "config_writer_file") {
        uint32_t last_word = r.line_words_.size() - 1;
        const char *filename = r.line_words_[last_word].c_str();
        iface->print_using_model(klass,
                schema_mgr_->model_.get(),
                rsp->xml_response->xml_blob,
                words[1].c_str(),
                filename); // Print to stdout
      } else {
        iface->pretty_print(klass, rsp->xml_response->xml_blob,words[1].c_str());
      }
      status = RW_STATUS_SUCCESS;
    }
  }

  return status;
}

/**
 * Handle the base-CLI command callback.  This callback will receive
 * notification of all syntactically correct command lines.  It should
 * return false if there are any errors.
 */
bool RwCLI::handle_command(ParseLineResult* r)
{
  /* If this is a built in command, handle it */
  if (handle_built_in_commands (r)) {
    return true;
  }

  process_cli_command (*r);
  
  // get the xml for the current command
  ParseNode *pn = r->result_tree_->children_.begin()->get();
  RW_ASSERT(pn);

  if (pn->is_config())
  {
    // The config command needs to update the xml that is stored in the mode.
    // Let the mode handle this function

    bool ret = (nullptr != parser->current_mode_->update_xml_cfg(config, r->result_tree_));
    return ret;
  }

  return true;
}

/*
 * function wrappers that are needed to register callbacks for ParseNodeFunctionals
 */


/**
 * Add the builtin commad parse nodes etc to the parser
 */
void RwCLI::add_builtin_cmds()
{

  // load cli manifest <name>

  /*  !! * START OF LOAD * !!*/
  ParseNodeFunctional *load_node = new ParseNodeFunctional (*parser, "load", parser->root_parse_node_.get());
  const char* help = "load data into the system";
  load_node->help_short_set(help);
  load_node->help_full_set (help);
  
  ParseNodeFunctional *load_cli = new ParseNodeFunctional (*parser, "cli", load_node);
  help =  "load manifest";
  load_cli->help_short_set (help);
  load_cli->help_full_set (help);
  
  ParseNodeFunctional *load_manifest = new ParseNodeFunctional (*parser, "manifest", load_cli);
  help = " manifest for cli";
  load_cli->help_full_set (help);
  load_cli->help_short_set (help);
  
  ParseNodeValueInternal *cli_m_name = new ParseNodeValueInternal (*parser, RW_YANG_LEAF_TYPE_STRING, load_manifest);

  help = "name of manifest file";
  cli_m_name->help_full_set (help);
  cli_m_name->help_short_set (help);
      
  load_manifest->set_callback(new CallbackRwCli(*this, &RwCLI::load_manifest));
  load_manifest->set_value_node (cli_m_name);

  load_cli->add_descendent (load_manifest);
  load_node->add_descendent (load_cli);
  
  // prevent accidental use
  cli_m_name = nullptr;
  load_cli = nullptr;

  // load config filename <name>
  ParseNodeFunctional *load_config = new ParseNodeFunctional (*parser, "config", load_node);
  help = "load configuration";
  load_config->help_short_set (help);
  load_config->help_full_set (help);
  
  ParseNodeFunctional *file_keyword = new ParseNodeFunctional (*parser, "filename", load_config);
  help = "text file to load";
  file_keyword->help_short_set (help);
  file_keyword->help_full_set (help);
  
  ParseNodeValueInternal *name_string = new ParseNodeValueInternal (*parser, RW_YANG_LEAF_TYPE_STRING, load_config);
  name_string->help_short_set (help);
  name_string->help_full_set (help);

  file_keyword->set_callback(new CallbackRwCli(*this, &RwCLI::load_config));
  file_keyword->set_value_node (name_string);

  // Nodes for xml config loading
  ParseNodeFunctional *xml_file_keyword = new ParseNodeFunctional (*parser, "xml", load_config);
  help = "xml file to load";
  xml_file_keyword->help_short_set (help);
  xml_file_keyword->help_full_set (help);
  
  ParseNodeValueInternal *xml_name_string = new ParseNodeValueInternal (*parser, RW_YANG_LEAF_TYPE_STRING, load_config);
  xml_name_string->help_short_set (help);
  xml_name_string->help_full_set (help);

  xml_file_keyword->set_callback(new CallbackRwCli(*this, &RwCLI::load_config_xml));
  xml_file_keyword->set_value_node (xml_name_string);

  load_config->add_descendent (file_keyword);
  load_config->add_descendent(xml_file_keyword);
  load_node->add_descendent (load_config);

  //prevent accidental use
  name_string = nullptr;
  file_keyword = nullptr;
  load_config = nullptr;

  parser->add_top_level_functional(load_node);
  load_node = nullptr;
  /*  !! * END OF LOAD * !!*/


  /* !! * START OF HELP * !! */
  ParseNodeFunctional *help_node = new ParseNodeFunctional (*parser, "help", parser->root_parse_node_.get());
  help = "show help for commands";
  help_node->help_short_set (help);
  help_node->help_full_set (help);
  
  ParseNodeFunctional *help_cmd = new ParseNodeFunctional (*parser, "command", help_node);
  help = "command for which help is requested";
  help_cmd->help_short_set (help);
  help_cmd->help_full_set (help);

  ParseNodeFunctional *name_keyword = new ParseNodeFunctional (*parser, "name", help_cmd);
  name_string = new ParseNodeValueInternal (*parser, RW_YANG_LEAF_TYPE_STRING, load_config);
  
  help_cmd->set_is_sentence(true);

  help_cmd->set_callback(new CallbackRwCli(*this, &RwCLI::help_command));

  name_keyword->set_value_node (name_string);
  help_cmd->add_descendent (name_keyword);
  help_node->add_descendent (help_cmd);

  help_cmd = nullptr;
  name_keyword = nullptr;

  parser->add_top_level_functional (help_node);
  help_node = nullptr;
  /* !! * END OF HELP * !! */

  /* !! * START OF SAVE *!! */
  ParseNodeFunctional *save_node = new ParseNodeFunctional (*parser, "save", parser->root_parse_node_.get());
  help = "save data";
  save_node->help_short_set (help);
  save_node->help_full_set (help);
  
  ParseNodeFunctional *cfg_keyword = new ParseNodeFunctional (*parser, "config", save_node);
  help = "Save the configuration file";
  cfg_keyword->help_short_set (help);
  cfg_keyword->help_full_set (help);

  ParseNodeFunctional *text_keyword = new ParseNodeFunctional (*parser, "text", cfg_keyword);
  help = "save configuration in text mode";
  text_keyword->help_short_set(help);
  text_keyword->help_full_set(help);
  text_keyword->set_cli_print_hook(config_print_to_file_hook_.c_str());

  text_keyword->set_callback(new CallbackRwCli(*this, &RwCLI::save_config));

  name_string = new ParseNodeValueInternal (*parser, RW_YANG_LEAF_TYPE_STRING, cfg_keyword);
  help = "name of file to use for save";
  name_string->help_short_set (help);
  name_string->help_full_set (help);
      
  text_keyword->set_value_node(name_string);
  cfg_keyword->add_descendent(text_keyword);

  ParseNodeFunctional *xml_keyword = new ParseNodeFunctional (*parser, "xml", cfg_keyword);
  help = "save configuration in xml mode";
  xml_keyword->help_short_set(help);
  xml_keyword->help_full_set(help);

  name_string = new ParseNodeValueInternal (*parser, RW_YANG_LEAF_TYPE_STRING, cfg_keyword);
  help = "name of file to use for save";
  name_string->help_short_set (help);
  name_string->help_full_set (help);
      
  xml_keyword->set_value_node(name_string);
  xml_keyword->set_cli_print_hook(pretty_print_xml_to_file_hook_.c_str());
  xml_keyword->set_callback(new CallbackRwCli(*this, &RwCLI::save_config));

  cfg_keyword->add_descendent (xml_keyword);
  save_node->add_descendent(cfg_keyword);

  parser->add_top_level_functional (save_node);
  /* !! * END OF SAVE * !! */

  // set-cli transport netconf/rwmsg
  ParseNodeFunctional *set_cli = new ParseNodeFunctional(*parser, "set-cli",
                                        parser->root_parse_node_.get());
  help = "Set CLI Parameters";
  set_cli->help_short_set(help);
  set_cli->help_full_set(help);

  ParseNodeFunctional *sc_transport = new ParseNodeFunctional(*parser, "transport",
                                        set_cli);
  help = "Set CLI Transport Mode";
  sc_transport->help_short_set(help);
  sc_transport->help_full_set(help);

  ParseNodeFunctional *sc_netconf = new ParseNodeFunctional(*parser, "netconf",
                                        sc_transport);
  help = "Set CLI Transport Mode to NETCONF";
  sc_netconf->help_short_set(help);
  sc_netconf->help_full_set(help);
  sc_netconf->set_callback(new CallbackRwCli(*this, &RwCLI::set_cli_transport));

  ParseNodeFunctional *sc_rwmsg = new ParseNodeFunctional(*parser, "rwmsg",
                                        sc_transport);
  help = "Set CLI Transport Mode to RW.MSG";
  sc_rwmsg->help_short_set(help);
  sc_rwmsg->help_full_set(help);
  sc_rwmsg->set_callback(new CallbackRwCli(*this, &RwCLI::set_cli_transport));

  sc_transport->add_descendent(sc_netconf);
  sc_transport->add_descendent(sc_rwmsg);
  set_cli->add_descendent(sc_transport);
  parser->add_top_level_functional(set_cli);
}

bool RwCLI::load_manifest_files()
{
  std::string cli_dir = schema_mgr_->get_cli_manifest_dir();
  if (!fs::exists(cli_dir)) {
    return false;
  }

  for ( fs::directory_iterator it(cli_dir); it != fs::directory_iterator(); ++it ) {

    auto& path = it->path();
    fs::path fn = path.filename();
    std::string ext;

    if (fn == RW_CLI_BASE_MANIFEST) {
      // Base manifest is special. Base manifest will not be loaded if already
      // loaded
      manifest_->read_base_cli_manifest(path.string().c_str());
      continue;
    }

    for (; !fn.extension().empty(); fn = fn.stem()) {
      ext = fn.extension().string() + ext;
    }

    if (ext == ".cli.xml") {
      if (manifest_->read_cli_manifest(path.string().c_str()) != RW_STATUS_SUCCESS) {
        return false;
      }
    }
  }

  return true;
}

bool RwCLI::load_manifest(const char *name)
{
  /// Load the objects from cli manifest into the CLI
  auto status = manifest_->read_cli_manifest(name);
  return (status == RW_STATUS_SUCCESS);
}


bool RwCLI::load_manifest(const ParseLineResult& r)
{
  /// Construct a new manifest
  RW_BCLI_MIN_ARGC_CHECK(&r, 4);
  return (load_manifest(r.line_words_[3].c_str()));
}

/**
 * Load an xml manifest file to modify the CLI.
 * @param[in] name: name of the xml file
 * @returns RW_STATUS_SUCCESS on success
 * @returns RW_STATUS_FAILURE on failure
 */

bool RwCLI::load_config(const ParseLineResult& r)
{
  RW_BCLI_MIN_ARGC_CHECK(&r, 4);
  const char *name = (r.line_words_[3].c_str());
  std::ifstream infile(name);

  if (infile.is_open()) {
    parser->auto_commit_ = true;
    std::string line;
    while (std::getline(infile, line))
    {
      parser->parse_line_buffer((const std::string&)line, false);
    }
    parser->auto_commit_ = false;
  }
  else {
    std::cout << "Error: failed to load " << name << std::endl;
  }

  return true;
}

bool RwCLI::load_config_xml(const ParseLineResult& r)
{
  RW_BCLI_MIN_ARGC_CHECK(&r, 4);
  const char *name = (r.line_words_[3].c_str());
  std::ifstream infile(name);

  if (infile.fail()) {
    std::cout << "Error: failed to load " << name << std::endl;
    return false;
  }

  infile.seekg(0, infile.end);
  uint32_t file_size = infile.tellg();
  infile.seekg(0, infile.beg);

  char* buf = new char[file_size+1];
  infile.read(buf, file_size);
  buf[file_size] = 0;

  // TODO check the xml file
  NetconfRsp *rsp = nullptr;
  rw_status_t rs = process_cli_command(buf, &rsp, nc_edit_config);
  if (rs != RW_STATUS_SUCCESS) {
    std::cerr << "Error: failed to execute the command" << std::endl;
    infile.close();
    delete[] buf;
    return false;
  }

  bool success = process_rpc_response(r, nc_edit_config, ec_merge, rsp);
  if (has_candidate_store_ && success) {
    // Commit the changes
    ParseLineResult res(*parser, "commit", ParseLineResult::ENTERKEY_MODE);
    parser->uncommitted_changes_ = false;
    parser->commit_behavioral(res);
  }

  infile.close();
  delete[] buf;

  return true;
}

/**
 * Allocate the vx framework
 * Setup the plugin and repository search paths
 * Load the RWCommonInterface
 */
rw_status_t RwCLI::initialize_plugin_framework(void)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  /*
   * Allocate a RIFT_VX framework to run the test
   * NOTE: this initializes a LIBPEAS instance
   */
  vxfp = rw_vx_framework_alloc();
  RW_ASSERT(vxfp);

  return status;
}

void RwCLI::print_element_names(xmlNode *node, int indent)
{
  xmlNode *cur_node = nullptr;
  int indent_increment = 0;

  for (cur_node = node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if ((strcmp((char *)cur_node->name, "message") != 0) &&
          (strcmp((char *)cur_node->name, "query") != 0)) {
        CLI_PRINTF("\n%*s%s: ", indent, " ", cur_node->name);
        indent_increment = 2;
      }
    } else if (cur_node->type == XML_TEXT_NODE) {
      int len = strlen((char *)cur_node->content);
      int idx = strspn((char *)cur_node->content, " \t\r\n");
      if (len != idx) {
        /* context is not whitespace, lets print it */
        CLI_PRINTF("%s", cur_node->content);
        indent_increment = 2;
      }
    }

    print_element_names(cur_node->children, indent+indent_increment);
  }
}

void RwCLI::display_xml_response(char *response)
{
  xmlDoc *doc = nullptr;
  xmlNode *root_element = nullptr;

  doc = xmlReadMemory(response, strlen(response), "noname.xml", nullptr, 0);
  if (doc == nullptr) {
    CLI_PRINTF("Error: could not parse the response\n");
  }

  /* Get the root element node */
  root_element = xmlDocGetRootElement(doc);

  /* print the tree */
  print_element_names(root_element, 0);
  printf("\n");
}

void RwCLI::set_messaging_hook(rwcli_messaging_hook hook)
{
  messaging_hook_ = hook;
}

void RwCLI::set_history_hook(rwcli_history_hook hook)
{
  history_hook_ = hook;
}

bool RwCLI::pretty_print_xml(char* xml_str, std::ostream& out)
{
  xmlDoc *doc;
  uint32_t xml_size = strlen(xml_str);
  doc = xmlReadMemory(xml_str, xml_size, nullptr, nullptr, 0);
  if (doc == nullptr) {
    std::cerr << "Failed to print XML. XML may be malformed" << std::endl;
    return false;
  }

  xmlChar *xml_buf = nullptr;
  int dump_size = 0;
  xmlDocDumpFormatMemory(doc, &xml_buf, &dump_size, 1);

  if (xml_buf != nullptr) {
    out << xml_buf << std::endl;
  }

  xmlFree(xml_buf);
  xmlFreeDoc(doc);
  return true;
}

/*****************************************************************************/

/**
 * The CallbackBaseCli constructor
 */

CallbackRwCli::CallbackRwCli(RwCLI& data, RwCliCallback cb)
    :cb_func_(cb), cb_data_ (&data)
{}

/**
 * The execute method - call the callback function
 */
bool CallbackRwCli::execute (const ParseLineResult &r)
{
  // ATTN: ugh
  return (*cb_data_.*cb_func_)(r);
}

__BEGIN_DECLS

/* 
 * RW.CLI C wrappers for use with zsh rift plugin.
 */

static RwCLI* rwcli_zsh_instance = NULL;

int rwcli_zsh_plugin_init(
        rwcli_transport_mode_t transport_mode,
        rwcli_user_mode_t user_mode)
{
  rw_status_t status;

  rwcli_zsh_instance = new RwCLI(transport_mode, user_mode);

  status = rwcli_zsh_instance->initialize_plugin_framework();
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  std::string base_manifest = rwcli_zsh_instance->schema_mgr_->get_cli_manifest_dir();
  base_manifest += "/";
  base_manifest += RW_CLI_BASE_MANIFEST;

  rwcli_zsh_instance->initialize(base_manifest.c_str());

  rwcli_zsh_instance->component = NULL;
  rwcli_zsh_instance->instance = NULL;

  rwcli_zsh_instance->load_manifest_files();

  return 0;
}

int rwcli_zsh_plugin_start()
{
  return 0;
}

int rwcli_zsh_plugin_deinit()
{
  if (rwcli_zsh_instance) {
    delete rwcli_zsh_instance;
    rwcli_zsh_instance = NULL;
  }
  return 0;
}

int rwcli_lookup(const char* cmd)
{
  rwcli_zsh_instance->reset_yang_model();

  // TODO support for modes
  std::string lcmd(cmd);
  lcmd += " ";
  ParseLineResult plr(*(rwcli_zsh_instance->parser), lcmd, ParseLineResult::NO_OPTIONS);

  return plr.success_;
}

char* rwcli_tab_complete(const char* line)
{
  rwcli_zsh_instance->reset_yang_model();

  std::string new_line;
  new_line = rwcli_zsh_instance->parser->tab_complete(line);
  return new_line.empty() ? NULL : strdup(new_line.c_str());
}

int rwcli_generate_help(const char* line)
{
  rwcli_zsh_instance->reset_yang_model();

  return rwcli_zsh_instance->parser->generate_help(line) ? 0 : -1;
}

int rwcli_tab_complete_with_matches(const char* line, rwcli_complete_matches_t **matches)
{
  rwcli_zsh_instance->reset_yang_model();

  *matches = NULL;
  std::vector<ParseMatch> res = rwcli_zsh_instance->parser->generate_matches(line);

  if (!res.empty()) {
    RWCLI_INIT_MATCHES((*matches), res.size()); 
    for (unsigned i = 0; i < res.size(); i++) {
      RWCLI_ADD_MATCH((*matches), i, 
          res[i].match_.c_str(), res[i].description_.c_str());
    }
  }

  return res.size();
}

int rwcli_exec_command(int argc, const char* const* cmd)
{
  const bool interactive = true;
  rwcli_zsh_instance->parser->process_line_buffer(argc, cmd, interactive);

  return 0;
}

const char* rwcli_get_prompt()
{
  if (!rwcli_zsh_instance) {
    return NULL;
  }
  return rwcli_zsh_instance->parser->prompt_;
}

void rwcli_set_messaging_hook(rwcli_messaging_hook hook)
{
  RW_ASSERT(rwcli_zsh_instance);
  rwcli_zsh_instance->set_messaging_hook(hook);
}

void rwcli_set_history_hook(rwcli_history_hook hook)
{
  RW_ASSERT(rwcli_zsh_instance);
  rwcli_zsh_instance->set_history_hook(hook);
}

__END_DECLS
