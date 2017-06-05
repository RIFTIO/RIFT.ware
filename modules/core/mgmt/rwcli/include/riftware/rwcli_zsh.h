/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwcli_zsh.h
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 07/15/2015 
 * @brief 
 *  The Rift plugin for zsh uses these exports to invoke RW.CLI methods.
 *  Serves as the C wrappers for RW.CLI cpp methods.
 */

#ifndef __RWCLI_ZSH_H__
#define __RWCLI_ZSH_H__

#include <stdlib.h>
#include "rwuagent.pb-c.h"
#include "rwcli.h"

__BEGIN_DECLS

#ifndef RW_CLI_MAX_ARGS
/**
 * Max number of arguments or words in a 
 * CLI command supported by RW.CLI
 */
#define RW_CLI_MAX_ARGS 1024
#endif

/**
 * RW.CLI's transport mode.
 */
typedef enum {
  RWCLI_TRANSPORT_MODE_RWMSG,   /**< Transport mode is RW.MSG */
  RWCLI_TRANSPORT_MODE_NETCONF, /**< Using NETCONF transport */

  RWCLI_TRANSPORT_MODE_MAX
} rwcli_transport_mode_t;

/**
 * RW.CLI user mode - can be admin, operator or none
 */
typedef enum {
  RWCLI_USER_MODE_INVALID, /**< Invalid auth credentials */
  RWCLI_USER_MODE_NONE,    /**< User mode not applicable */
  RWCLI_USER_MODE_ADMIN,   /**< User has admin rights */
  RWCLI_USER_MODE_OPER     /**< User has operator rights */
} rwcli_user_mode_t;

/**
 * RW.CLI Message type. This will be sent to the agent. Payload sent to the cli
 * agent will be determined by the message type. For Netconf message type the
 * payload is NetconfReq protobuf. For internal messages rwcli_internal_req_t is
 * the payload.
 */
typedef enum {
  /** 
   * Netconf message, payload is NetconfReq protobuf. Sent from the cli agent
   * to Netconf server or RW.MgmtAgent. 
   */
  RWCLI_MSG_TYPE_RW_NETCONF, 

  /**
   * Internal message sent to the agent to get the transport status and
   * statistics. Does not have a request payload. The agent returns a formatted
   * printable string as response.
   */
  RWCLI_MSG_TYPE_GET_TRANSPORT_STATUS,

  /**
   * Changes the current CLI transport. The request payload is 
   * rwcli_internal_req_t.transport_mode, specifies the transport mode to be
   * changed. This message is consumed by the cli controller.
   */
  RWCLI_MSG_TYPE_SET_CLI_TRANPORT
 
} rwcli_msg_type_t;

/**
 * Payload used for internal requests from rw.cli plugin to controller/agent
 */
typedef struct rwcli_internal_req_s {
  union {
    uint32_t transport_mode; /**< Used for RWCLI_MSG_TYPE_SET_CLI_TRANPORT */
  } u;
} rwcli_internal_req_t;

/** 
 * Payload used for response from agent to controller/rw.cli plugin
 */
typedef struct rwcli_internal_rsp_s {
  uint32_t status;
  uint32_t str_data_len;
  char     str_data[0];
} rwcli_internal_rsp_t;

/**
 * Used for returning the list of matches when a <TAB> key is pressed
 */
typedef struct rwcli_complete_matches_s {
  char* match_str; 
  char* description;
  // Grouping fields to be added later
} rwcli_complete_matches_t;

/* Hook that will be called back to the zsh rift module, when rw.cli executes
 * a command.  */
typedef rw_status_t (*rwcli_messaging_hook)(
                        rwcli_msg_type_t msg_type,
                        void* req, void **rsp);

/* Hook that will be invoked on the zsh rift module to display history */
typedef void (*rwcli_history_hook)(void);

/**
 * Initializes the rw.cli library for use with ZSH
 * @param[in] transport_mode  Transport mode in which the CLI is operating
 * @param[in] user_mode       Specifies if the user is an operator/admin
 * @returns 0 on success, -1 otherwise.
 */
int rwcli_zsh_plugin_init(
        rwcli_transport_mode_t transport_mode,
        rwcli_user_mode_t user_mode
    );

/**
 * Start the rw.cli library. Currently not used.
 */
int rwcli_zsh_plugin_start();

/**
 * Cleanups the resources used by rw.cli library
 */
int rwcli_zsh_plugin_deinit();

/**
 * Lookup the command (first word) to see if it is part of the yang model
 * in rw.cli
 * @param[in] cmd  first word of the command to be executed
 * @return
 *   Returns 1 (TRUE) when the command is part of yang model in rw.cli, 
 *   otherwise 0.
 */
int rwcli_lookup(const char* cmd);

/**
 * Uses the rw.cli yang model for command completion. In the method the tab
 * completion is performed by the rw.cli and it prints the completion output
 * on the stdout.
 * @param[in] line  command that is to be completed
 * @returns
 *   Returns the partially/fully completed command string
 */
char* rwcli_tab_complete(const char* line);

/**
 * Uses the rw.cli yang model for generating help. In the method the generating
 * help is performed by the rw.cli and it prints the completion output on the 
 * stdout.
 * @param[in] line  line for which help is to be provides
 * @returns 0 on success -1 on error
 */
int rwcli_generate_help(const char* line);

/**
 * Fetches the list of possible completions for the given command line. The
 * list of possible matches along with description are returned as an array.
 * The caller is expected to free the matches (use RWCLI_FREE_MATCHES macro).
 * @param[in]  line     command for which completions are to be returned
 * @param[out] matches  array of matches for the given input
 * @returns
 *   Returns the number of matches
 */
int rwcli_tab_complete_with_matches(const char* line, 
                  rwcli_complete_matches_t **matches);

/**
 * Executes the given rw.command. rw.cli library parses the command as per the
 * yang model, converts it to xml and invokes the messaging hook.
 * @param[in] argc Number of words in the command
 * @param[in] argv 2D array containing command tokens 
 * @return
 *   Returns 0 on success.
 */
int rwcli_exec_command(int argc, const char* const* argv);

/**
 * RW.Shell needs to communicate with the uagent/confd to execute commands. On
 * setting this hook, the rw.cli lib will invokes this hook when a message is
 * to be sent.
 * @param[in]  hook   Hook function that will be called when a command requires
 *                    messaging.
 */
void rwcli_set_messaging_hook(rwcli_messaging_hook hook);

/**
 * CLI history will be stored in the zsh. To show history, the zsh plugin needs
 * to be invoked to display the history.
 * @param[in]  hook  Hook function that will be invoked when command history is
 *                   to be invoked.
 */
void rwcli_set_history_hook(rwcli_history_hook hook);

/**
 * Get the CLI prompt as maintained by the rw.cli library. 
 * The same prompt will be set as the ZSH prompt.
 * @return
 *   RW.CLI prompt will be returned
 */
const char* rwcli_get_prompt();

/* Macros for manipulating rwcli_complete_matches_t structure */
#define RWCLI_INIT_MATCHES(_match_, _num_matches_) \
  _match_ = (rwcli_complete_matches_t*)calloc(_num_matches_, sizeof(rwcli_complete_matches_t));

#define RWCLI_ADD_MATCH(_match_, _index_, _match_str_, _descr_) \
{\
  _match_[_index_].match_str = strdup(_match_str_); \
  _match_[_index_].description = strdup(_descr_); \
}

#define RWCLI_FREE_MATCHES(_match_, _size_) \
{\
  int _i; \
  for (_i = 0; _i < _size_; _i++) { \
    free(_match_[_i].match_str); \
    free(_match_[_i].description); \
  } \
  free(_match_); \
}

__END_DECLS

#endif /* __RWCLI_ZSH_H__ */
