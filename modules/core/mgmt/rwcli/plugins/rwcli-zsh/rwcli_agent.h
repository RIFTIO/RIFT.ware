/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwcli_agent.h
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 07/15/2015 
 * @brief Defines the CLI Agent Channel related structures
 *
 */

#ifndef __RWCLI_AGENT_H__
#define __RWCLI_AGENT_H__

#include "rwcli_zsh.h"

#ifdef alloca
# undef alloca
#endif
#ifdef UNUSED
# undef UNUSED
#endif

/* Required for Widget and other definitions from ZLE */
#include "zle.mdh"

#define RW_CLI_MAX_PIPES 4
#define RW_CLI_NO_MORE RW_CLI_MAX_PIPES
#define RW_CLI_NOT_FREE (-1)
#define RW_CLI_MAX_BUF (256 * 1024)

/**
 * PIPE file descriptors
 */
typedef union rwcli_pipe_s {
  int fds[2];
  struct {
    int read;
    int write;
  }fd;
} rwcli_pipe_t;

/**
 * A Message Channel between the controller and an agent.
 */
typedef struct rwcli_msg_channel_s {
  rwcli_pipe_t in;   /**< Pipe from Controller to Agent */
  rwcli_pipe_t out;  /**< Pipe from agent to Controller */
} rwcli_msg_channel_t;

/**
 * Struct containing the RW.CLI Controller fields.
 */
typedef struct rwcli_controller_s
{
  /**
   * Protopbuf decoded Netconf Response. Stored in the controller for freeing
   * it after the command is executed.
   */
  NetconfRsp*            resp;

  /** Receive buffer used for storing message received on the Channel */
  uint8_t*               recv_buf;

  /** Size of the receive buffer*/
  size_t                 recv_buf_len;

  /** RW.Trace context */
  rwtrace_ctx_t*         trace_ctxt;

  /** Message channels - Netconf and RW.Msg channels */
  rwcli_msg_channel_t    agent_channels[RWCLI_TRANSPORT_MODE_MAX];

  /** Agent transport mode. The active agent type is stored here */
  rwcli_transport_mode_t agent_type;

  /** Specifies if the Netconf Agent module is loaded */
  bool                   is_netconf_agent_loaded; 

  /* ZSH Widgets and Thingy */
  Widget w_comp; /**< Widget used for tab completion */
  Widget w_gen_help; /**< Widget used for generating help */
  Thingy t_orig_tab_bind; /**< Original thingy used for tab completion */
} rwcli_controller_t;

/**
 * Header to the message sent from Controller to Agent.
 */
typedef struct rwcli_agent_msg_hdr_s
{
  uint32_t   msg_len;  /**< Length the message. Doesn't include this header */ 
  uint32_t   msg_type; /**< Message Type */
} rwcli_agent_msg_hdr_t;


/**
 * RW.CLI Controller instance.
 */
extern rwcli_controller_t rwcli_controller;

#endif /* __RWCLI_AGENT_H__ */
