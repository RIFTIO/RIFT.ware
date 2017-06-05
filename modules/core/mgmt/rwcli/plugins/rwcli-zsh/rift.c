/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rift.cpp
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 07/15/2015
 * @brief ZSH module to parse and execute rw.commands 
 */

/* For asprintf */
#define _GNU_SOURCE 1
#include <stdio.h>
#undef _GNU_SOURCE

#include "rwcli_agent.h"

#include <poll.h>
#include <openssl/sha.h>

/* These two are generated from the rift.mdd file by mkmakemod.sh */
#include "rift.mdh"
#include "rift.pro"

#define RW_DEFAULT_MANIFEST "cli_rwfpath.xml"

extern rift_cmdargs_t rift_cmdargs;

static int rift_exec(char *nam, char **argv, Options ops, int func);

/* Before executing the command, the rift plugin returns this builtin, so that
 * we get a callback to execute the command */
static struct builtin rw_command_bn =
    BUILTIN("rw", BINF_RW_CMD | BINF_NOGLOB, rift_exec, 0, -1, -1, NULL, NULL);

/*
 * RW.CLI Controller Instance
 */
rwcli_controller_t rwcli_controller;

/**
 * Initializes the message channel between agent and the CLI.
 */
static void msg_channel_init(rwcli_msg_channel_t* ch)
{
  ch->in.fd.read = -1;
  ch->in.fd.write = -1;
  ch->out.fd.read = -1;
  ch->out.fd.write = -1;
}

/**
 * Creates the pipes to communicate with the agent
 */
static int msg_channel_create(rwcli_msg_channel_t* ch) {
  int ret = pipe(ch->in.fds);
  if (ret != 0) {
    RWTRACE_CRIT(rwcli_controller.trace_ctxt, RWTRACE_CATEGORY_RWCLI,
        "Error creating IN pipe: %s", strerror(errno));
    return -1;
  }

  ret = pipe(ch->out.fds);
  if (ret != 0) {
    RWTRACE_CRIT(rwcli_controller.trace_ctxt, RWTRACE_CATEGORY_RWCLI,
        "Error creating OUT pipe: %s", strerror(errno));
    return -1;
  }
  return 0;
}

/**
 * Initializes the RW.TRACE context and sets the severity as specified in the
 * command line.
 */
static void controller_init_trace(rwcli_controller_t* inst)
{
  inst->trace_ctxt = rwtrace_init();
  RW_ASSERT(inst->trace_ctxt);

  if (rift_cmdargs.trace_level == -1) {
    // trace level not set, using ERROR as default
    rift_cmdargs.trace_level = RWTRACE_SEVERITY_ERROR;
  }

  rwtrace_ctx_category_destination_set(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
                RWTRACE_DESTINATION_CONSOLE);
  rwtrace_ctx_category_severity_set(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
                (rwtrace_category_t)rift_cmdargs.trace_level);
}

/*
 * Initialize and create a specific agent channel.
 */
static void controller_agent_channel_init(
                rwcli_controller_t* inst, 
                rwcli_transport_mode_t type)
{
  rwcli_msg_channel_t* agent_ch = &(inst->agent_channels[type]);
  msg_channel_init(agent_ch);
  msg_channel_create(agent_ch);
}

/*
 * Initializes the RW.CLI controller.
 */
static void controller_init(rwcli_controller_t* inst)
{
  memset(inst, 0, sizeof(rwcli_controller_t));

  // Initialize the trace module and set appropriate severity
  controller_init_trace(inst);

  // Allocate the receive buffer
  inst->recv_buf = (uint8_t*)calloc(sizeof(uint8_t), RW_CLI_MAX_BUF);
  inst->recv_buf_len = RW_CLI_MAX_BUF;

  if (!getenv("RIFT_VAR_ROOT")) {
    if (rift_cmdargs.use_rift_var_root == -1) {
      char *rift_var_root = NULL;
      asprintf(&rift_var_root, "%s/var/rift", getenv("RIFT_INSTALL"));
      setenv("RIFT_VAR_ROOT", rift_var_root, true);
      free(rift_var_root);
    } else {
      setenv("RIFT_VAR_ROOT", rift_cmdargs.rift_var_root, true);
    }
  }

  RWTRACE_INFO(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "RIFT_VAR_ROOT=%s", getenv("RIFT_VAR_ROOT"));

  // Decide the RW.CLI communication mode with RW.MgmtAgent
  if (rift_cmdargs.use_netconf == -1) {
    const char* agent_mode = rw_getenv("RWMGMT_AGENT_MODE");
    if (agent_mode && strcmp(agent_mode, "XML") == 0) {
      // If the agent mode is XML only RW.MSG mode is possible. Override the
      // command line option for netconf and opt for RW.MSG mode.
      inst->agent_type = RWCLI_TRANSPORT_MODE_RWMSG;
    } else {
      // Transport mode not chosen in command line and not XML mode. Choose
      // Netconf mode by default.
      inst->agent_type = RWCLI_TRANSPORT_MODE_NETCONF;
    }
  } else if (rift_cmdargs.use_netconf) {
    inst->agent_type = RWCLI_TRANSPORT_MODE_NETCONF;
  } else {
    inst->agent_type = RWCLI_TRANSPORT_MODE_RWMSG;
  }

  inst->is_netconf_agent_loaded = false;

  // To support background processes and job control, there should
  // be multiple channels available

  // Create two agent channels one for RWMSG and the other for Netconf
  controller_agent_channel_init(inst, RWCLI_TRANSPORT_MODE_RWMSG);
  controller_agent_channel_init(inst, RWCLI_TRANSPORT_MODE_NETCONF);
}

/**
 * Recevie a message from the agent.
 *
 * In the message the first 4bytes provide the message length. The rest is the
 * NetconfResp protobuf or rwcli_internal_rsp_t.
 *
 * @param[in] inst       - RW.CLI controller instance
 * @param[in] agent_type - Agent type (netconf/rwmsg) from which message to be
 *                         received.
 * @param[out] msg_len    - Length for the received message.
 *
 * @returns RW_STATUS_SUCCESS on success, RW_STATUS_FAILURE otherwise.
 */
static rw_status_t controller_recv_from_agent(
                      rwcli_controller_t* inst,
                      rwcli_transport_mode_t agent_type, 
                      unsigned *msg_len)
{
  ssize_t nread = read(inst->agent_channels[agent_type].out.fd.read, 
                       msg_len, sizeof(unsigned));
  if (nread <= 0) {
    if (errno != EAGAIN && errno != EINTR) {
      RWTRACE_ERROR(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nSHELL: Error reading from CLI-AGENT: %s", strerror(errno));
    }
    return RW_STATUS_FAILURE;
  }
  if (*msg_len == 0) {
    // no reply
    return RW_STATUS_FAILURE;
  }
  if (*msg_len > inst->recv_buf_len) {
    free(inst->recv_buf);
    inst->recv_buf = (uint8_t*)calloc(sizeof(uint8_t), *msg_len);
    inst->recv_buf_len = *msg_len;
  }

  // Read until we received the complete message
  size_t nr = 0;
  size_t remaining = *msg_len;
  nread = 0;
  do {
    nr = read(inst->agent_channels[agent_type].out.fd.read, 
              inst->recv_buf + nread, remaining);
    if (nr <= 0) {
      RWTRACE_ERROR(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nSHELL: Error reading from CLI-AGENT %s\n", strerror(errno));
      return RW_STATUS_FAILURE;
    }
    nread += nr;
    remaining -= nr;
    RW_ASSERT(remaining >= 0);
  } while(remaining);
 
  return RW_STATUS_SUCCESS; 
}

/*
 * Consume the previously unread responses from the agent.
 *
 * This method is required when the user presses Control-C when an operation is
 * being performed and the response is not received. Before performing the next
 * operation this method is used to flush the read stream.
 * 
 */
static void controller_consume_unread_agent_messages(
                  rwcli_controller_t* inst,
                  rwcli_transport_mode_t agent_type)
{
  unsigned msg_len = 0;
  rw_status_t status = RW_STATUS_SUCCESS;
  int ret = 0;
  struct pollfd fds[] = {
    { inst->agent_channels[agent_type].out.fd.read, POLLIN, 0}
  };

  // Consume the messages until there is none to read
  while (1) {
    // Check if read is set on the fd and return immediately
    ret = poll(fds, 1, 0);
    if (ret == 0) {
      // No events
      return;
    }
    
    status = controller_recv_from_agent(inst, agent_type, &msg_len);
    if (status != RW_STATUS_SUCCESS) {
      return;
    }
  }
}

/**
 * Checks if the given agent channel is initialized.
 */
static bool controller_is_channel_initialized(
                    rwcli_controller_t* inst,
                    rwcli_transport_mode_t agent_type)
{
  return (inst->agent_channels[agent_type].in.fd.write != -1);
}

/**
 * Sends message from controlelr to the specified agent. The message payload can
 * be NULL, in which case only the header will be sent.
 *
 * @param[in] inst - Controller instance
 * @param[in] agent_type - Agent to which the message to be sent
 * @param[in] msg_type   - Message type
 * @param[in] msg_buf    - Message payload (encoded)
 * @param[in] msg_len    - Size of the message payload
 *
 * @returns RW_STATUS_SUCCESS on success, RW_STATUS_FAILURE otherwise.
 */
static rw_status_t controller_send_to_agent(
                      rwcli_controller_t* inst,
                      rwcli_transport_mode_t agent_type,
                      rwcli_msg_type_t msg_type,
                      uint8_t* msg_buf,
                      unsigned msg_len)
{
  int nw = 0;
  rwcli_agent_msg_hdr_t msg_hdr = {
    .msg_len = msg_len,
    .msg_type = msg_type
  };

  // First write the header
  nw = write(inst->agent_channels[agent_type].in.fd.write, 
            (const void*)(&msg_hdr), sizeof(rwcli_agent_msg_hdr_t));
  if (nw <= 0) {
    RWTRACE_ERROR(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nSHELL: Error writing to CLI-AGENT %s\n", strerror(errno));
    return RW_STATUS_FAILURE;
  }

  if (msg_buf) {
    nw = write(inst->agent_channels[agent_type].in.fd.write, msg_buf, msg_len);
    if (nw <= 0) {
      RWTRACE_ERROR(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nSHELL: Error writing to CLI-AGENT %s\n", strerror(errno));
      return RW_STATUS_FAILURE;
    }
  }

  return  RW_STATUS_SUCCESS;
}

/**
 * Executes a request towards the specified agent and receives the response.
 * The response is stored in rwcli_controller.recv_buf.
 *
 * @param[in] inst - Controller instance
 * @param[in] agent_type - Agent on which the command is to be executed 
 * @param[in] msg_type   - Message type
 * @param[in] msg_buf    - Reqest payload (encoded)
 * @param[in] msg_len    - Size of the Request payload
 * @param[out] recv_msg_len - Size of the received message (encoded)
 *
 * @returns RW_STATUS_SUCCESS on success, RW_STATUS_FAILURE otherwise.
 */
static rw_status_t controller_execute(
                      rwcli_controller_t* inst,
                      rwcli_transport_mode_t agent_type,
                      rwcli_msg_type_t msg_type,
                      uint8_t*  msg_buf,
                      unsigned  msg_len,
                      unsigned* recv_msg_len)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  if (!controller_is_channel_initialized(inst, agent_type)) {
    RWTRACE_CRIT(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
        "Messaging not initialized, failed to execute the command");
    return RW_STATUS_FAILURE;
  }

  // Consume any previous unread messages on the stream
  controller_consume_unread_agent_messages(inst, agent_type);

  status = controller_send_to_agent(inst, agent_type, msg_type, msg_buf, msg_len);
  if (status != RW_STATUS_SUCCESS) {
    return status;
  }

  RWTRACE_DEBUG(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
      "\nSHELL: sent %d bytes to CLI-AGENT", msg_len);

  status = controller_recv_from_agent(inst, agent_type, recv_msg_len);
  if (status != RW_STATUS_SUCCESS) {
    return status;
  }

  RWTRACE_DEBUG(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
      "\nSHELL: received %u bytes from CLI-AGENT msglen\n", *recv_msg_len);

  return status;
}

/**
 * Handler for Netconf message type. Sends the encoded the NetconfReq protobuf
 * and receives the decoded NetconfRsp protobuf.
 *
 * @param[in] inst - RW.CLI Controller instance
 * @param[in] req  - Netconf Request decoded protobuf
 * @param[out] rsp - Netconf response (decoded)
 *
 * @returns RW_STATUS_SUCCESS on success, RW_STATUS_FAILURE otherwise.
 */
static rw_status_t controller_handle_rwnetconf_msg(
                    rwcli_controller_t* inst,
                    NetconfReq* req, NetconfRsp **rsp)
{
  unsigned recv_msg_len = 0;
  unsigned msg_len = netconf_req__get_packed_size(NULL, req);
  uint8_t msg_buf[msg_len];
  rw_status_t status;
 
  // Encode the protobuf
  netconf_req__pack(NULL, req, msg_buf);

  status = controller_execute(inst, inst->agent_type,
                              RWCLI_MSG_TYPE_RW_NETCONF,
                              msg_buf, msg_len, &recv_msg_len);
  if (status != RW_STATUS_SUCCESS) {
    *rsp = NULL;
    return status;
  }

  // Decode the response
  inst->resp = netconf_rsp__unpack(NULL, recv_msg_len, 
                                  inst->recv_buf);
  if (inst->resp == NULL) {
    RWTRACE_ERROR(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nReceived message unpack failed\n");
    *rsp = NULL;
    return RW_STATUS_FAILURE;
  }

  // The response is stored in the controller to be deleted later after the
  // response is processed by the RW.CLI plugin.
  *rsp = inst->resp;

  return RW_STATUS_SUCCESS;
}

/**
 * Handles the Get-transport-status message and prints the response received.
 *
 * In case of Netconf mode both Netconf transport and RW.MSG status is printed.
 * In case of RW.MSG mode only the RW.MSG transport status is printed.
 */
static rw_status_t controller_handle_get_transport_status(
                          rwcli_controller_t* inst)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  unsigned recv_msg_len = 0;
  rwcli_internal_rsp_t* rsp = NULL;

  // Sent the message to the available transports. 
  // If the mode is RW.MSG Netconf status is not required
  if (inst->agent_type == RWCLI_TRANSPORT_MODE_NETCONF) {
    status = controller_execute(inst, 
                RWCLI_TRANSPORT_MODE_NETCONF,
                RWCLI_MSG_TYPE_GET_TRANSPORT_STATUS,
                NULL, 0, &recv_msg_len);
    
    rsp = (rwcli_internal_rsp_t*)inst->recv_buf;
    printf("\n%.*s", rsp->str_data_len, rsp->str_data);
  }
  status = controller_execute(inst, 
                RWCLI_TRANSPORT_MODE_RWMSG,
                RWCLI_MSG_TYPE_GET_TRANSPORT_STATUS,
                NULL, 0, &recv_msg_len);
  rsp = (rwcli_internal_rsp_t*)inst->recv_buf;
  printf("\n%.*s\n", rsp->str_data_len, rsp->str_data);

  fflush(stdout);

  return status;
}

/**
 * Handler for set-cli-transport message. Controller's agent_type is changed and
 * in case of a switch to Netconf transport and if the NetconfAgent module is
 * not yet loaded, then loads the NetconfAgent module.
 */
static rw_status_t controller_set_cli_transport(
                        rwcli_controller_t* inst,
                        rwcli_transport_mode_t transport_mode)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  if (inst->agent_type == transport_mode) {
    // No change
    return status;
  }
  inst->agent_type = transport_mode;

  if (inst->agent_type == RWCLI_TRANSPORT_MODE_NETCONF &&
      !(inst->is_netconf_agent_loaded)) {
    if (load_module("zsh/rwnetconf_agent", NULL, 0) != 0) {
      inst->is_netconf_agent_loaded = false;
      printf("\nCRITICAL: Loading the netconf agent module failed\n");
      fflush(stdout);
      return RW_STATUS_FAILURE;
    }
    inst->is_netconf_agent_loaded = true;
  }

  return status;
}

/**
 * Handler for RW.CLI internal message
 */
static rw_status_t controller_handle_internal_msg(
                    rwcli_controller_t* inst,
                    rwcli_msg_type_t    msg_type,
                    rwcli_internal_req_t* req,
                    rwcli_internal_rsp_t** rsp)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  switch(msg_type) {
    case RWCLI_MSG_TYPE_GET_TRANSPORT_STATUS:
      status = controller_handle_get_transport_status(inst);
      break;
    case RWCLI_MSG_TYPE_SET_CLI_TRANPORT:
      status = controller_set_cli_transport(inst, req->u.transport_mode);
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  return status;
}

/**
 * Clear the current ZLE line
 */
static void trash_line() {
  unmetafy_line();
  trashzle();
  metafy_line();
}

/**
 * Widget method that will be called when a tab key is pressed.
 */
static int rift_complete(UNUSED(char** arg))
{
  int new_len = 0;
  char *new_line = NULL;
  int i = 0;

  /* Metafy - changes the zleline to string and stores in zlemetaline
   * zleline is not in string format */
  metafy_line();

  for (i = 0; zlemetaline[i] != '\0'; i++) {
    if (zlemetaline[i] == '|' || zlemetaline[i] == '>') {
      unmetafy_line();
      execzlefunc(rwcli_controller.t_orig_tab_bind, zlenoargs, 0);
      return 0;
    }
  }

  new_line = rwcli_tab_complete(zlemetaline);
  fflush(stdout);
  if (new_line) {
    new_len = strlen(new_line);
    if ((new_len > 1 && new_line[new_len - 1] != ' ') ||
        (zlemetall == new_len)) {
      trash_line();
    }

    // First truncate the zlemetaline and then insert the new line
    // zlemetacs is the cursor, zlemetall is the line length
    zlemetacs = 0;
    foredel(zlemetall, CUT_RAW);
    inststrlen(new_line, new_len, new_len);
    free(new_line);
  } else {
    trash_line();
    zlemetacs = 0;
    foredel(zlemetall, CUT_RAW);
  }

  unmetafy_line();
  /* zlemeta* is not valid beyond this point */

  return 0;
}

/**
 * Widget method that will be called when a tab key is pressed.
 */
static int rift_generate_help(UNUSED(char** arg))
{
  /* Metafy - changes the zleline to string and stores in zlemetaline
   * zleline is not in string format */
  metafy_line();

  rwcli_generate_help(zlemetaline);
  fflush(stdout);

  unmetafy_line();
  /* zlemeta* is not valid beyond this point */

  // trash the old line and redraw
  trashzle();

  return 0;
}

/**
 * This is hook function that will be called before executing a command
 * in the zsh pipeline. The first word of the command is used to check if it is
 * a rw.command. 
 *
 * @param[in] cmdarg  First word of the command
 * @returns
 * Returns the builtin structure with the callback rift_exec. When zsh executes
 * the builtin command, the callback rift_exec() will be invoked.
 */
static HashNode rift_lookup(char *cmdarg)
{
  if (rwcli_lookup(cmdarg)) {
    return &rw_command_bn.node;
  } else {
    return NULL;
  }
}

/**
 * This method will be invoked when the rw.command is executed by the zsh as a
 * builtin.
 * @param[in] nam   First word of the command to be executed
 * @param[in] args  Rest of the command words
 * @returns
 *  Returns 0 on successful execution, otherwise -1
 */
int rift_exec(char *nam, char **args, UNUSED(Options ops), UNUSED(int func))
{
  char* buf[RW_CLI_MAX_ARGS] = {0};
  int len = 0;
  rwcli_controller.resp = NULL;

  buf[0] = nam;
  while (*args) {
    buf[++len] = *args;
    args++;
    RW_ASSERT_MESSAGE (len < sizeof(buf)/sizeof(buf[0]),
        "Command arguments more than %d configured", RW_CLI_MAX_ARGS);
  }

  rwcli_exec_command(len + 1, (const char* const*)buf);
  fflush(stdout);

  if (rwcli_controller.resp) {
    netconf_rsp__free_unpacked(NULL, rwcli_controller.resp);
    rwcli_controller.resp = NULL;
  }

  return 0;
}

/**
 * This function will be called whenever the prompt is drawn by the zle.
 * Sets the zsh global variable 'prompt' which is equivalent to the env
 * PROMPT / PS1.
 */
static void rift_prompt(void)
{
  const char* rw_prompt = rwcli_get_prompt();
  if (rw_prompt) {
    zfree(prompt, 0);
    prompt = ztrdup(rw_prompt);
  }
}

/**
 * Authenticates the provided username and password agains the stored SHA1 Hash
 * values.
 * @param[in] cmdargs  Command line arguments struct containinng username and
 *                      password.
 * @returns User-mode based on the username. If authentication fails then 
 *          RWCLI_USER_MODE_INVALID is returned.
 */
rwcli_user_mode_t rift_authenticate(rift_cmdargs_t *cmdargs)
{
  static uint8_t admin_sha[] = {
    0xd0, 0x33, 0xe2, 0x2a, 0xe3, 0x48, 0xae, 0xb5, 0x66, 0x0f,
    0xc2, 0x14, 0x0a, 0xec, 0x35, 0x85, 0x0c, 0x4d, 0xa9, 0x97
  };
  static uint8_t oper_sha[]  = {
    0xd6, 0x3d, 0xec, 0xb2, 0x5a, 0x2e, 0x73, 0x69, 0x87, 0xdd,
    0x8b, 0xcd, 0x39, 0xe6, 0x5f, 0xb0, 0x12, 0x43, 0x79, 0xc0
  };
  uint8_t computed_sha[SHA_DIGEST_LENGTH] = {0};

  if (cmdargs->username == NULL || 
      cmdargs->passwd == NULL) {
    return RWCLI_USER_MODE_INVALID;
  }

  if (strcmp(cmdargs->username, "admin") == 0) {
    unsigned long len = strlen(cmdargs->passwd);
    SHA1((uint8_t*)cmdargs->passwd, len, computed_sha);
    if (memcmp(computed_sha, admin_sha, SHA_DIGEST_LENGTH) != 0) {
      // admin password mismatch
      return RWCLI_USER_MODE_INVALID;
    }
    return RWCLI_USER_MODE_ADMIN;
  } else if (strcmp(cmdargs->username, "oper") == 0) {
    unsigned long len = strlen(cmdargs->passwd);
    SHA1((uint8_t*)cmdargs->passwd, len, computed_sha);
    if (memcmp(computed_sha, oper_sha, SHA_DIGEST_LENGTH) != 0) {
      // oper password mismatch
      return RWCLI_USER_MODE_INVALID;
    }
    return RWCLI_USER_MODE_OPER;
  }
 
  return RWCLI_USER_MODE_INVALID;
}
/**
 * This method will be invoked whene the rw.cli executes a command and 
 * requires a transport to send the message.
 */
static rw_status_t messaging_hook(
                      rwcli_msg_type_t msg_type,
                      void* req, void **rsp)
                      //NetconfReq *req, NetconfRsp **rsp)
{
  if (msg_type == RWCLI_MSG_TYPE_RW_NETCONF) {
    return controller_handle_rwnetconf_msg(&rwcli_controller,
              (NetconfReq*)req, (NetconfRsp**)rsp);
  }

  return controller_handle_internal_msg(&rwcli_controller, msg_type,
              (rwcli_internal_req_t*)req, (rwcli_internal_rsp_t**)rsp);
}

void history_hook()
{
  char *argv[] = { NULL };
  struct options ops;

  memset(&ops, 0, sizeof(struct options));
  ops.ind[(int)'l'] = 1;

  // invoke the builtin function 'history' (internall uses fc -l)
  bin_fc("history", argv, &ops, BIN_FC);
}


/* Module Features. We don't have any. */
static struct features module_features = {
    NULL, 0,
    NULL, 0,
    NULL, 0,
    NULL, 0,
    0
};

/* This function is called when the module is loaded */

/**/
int
setup_(UNUSED(Module m))
{
    return 0;
}

/* This function is called after the setup, to get the capabilities of this
 * module. For RIFT we are not exposing any features. */

/**/
int
features_(Module m, char ***features)
{
    *features = featuresarray(m, &module_features);
    return 0;
}

/* This is called from within the featuresarray to enable the features */

/**/
int
enables_(Module m, int **enables)
{
    return handlefeatures(m, &module_features, enables);
}


/* Called after the features are enabled when the module is loaded.
 * Module initialization code goes here. */

/**/
int
boot_(Module m)
{
  char* dummy = NULL;

  controller_init(&rwcli_controller);

  // Authenticate in case of RW.MSG mode
  rwcli_user_mode_t user_mode = RWCLI_USER_MODE_NONE;
  if (rwcli_controller.agent_type == RWCLI_TRANSPORT_MODE_RWMSG) {
    user_mode = rift_authenticate(&rift_cmdargs);
    if (user_mode == RWCLI_USER_MODE_INVALID) {
      printf("\nERROR: Invalid username/password\n");
      exit(-1);
    }
  }

  rwcli_zsh_plugin_init(rwcli_controller.agent_type, user_mode);
  rwcli_set_messaging_hook(messaging_hook);
  rwcli_set_history_hook(history_hook);

  // Always load the rwmsg_agent module, If netconf is enabled then 
  // this module will only receive logging notifications
  if (load_module("zsh/rwmsg_agent", NULL, 0) != 0) {
    printf("\nCRITICAL: Loading the messaging agent module failed\n");
    fflush(stdout);
    return -1;
  }

  // Load the agent that is required
  if (rwcli_controller.agent_type == RWCLI_TRANSPORT_MODE_NETCONF) {
    if (load_module("zsh/rwnetconf_agent", NULL, 0) != 0) {
      printf("\nCRITICAL: Loading the netconf agent module failed\n");
      fflush(stdout);
      return -1;
    }
    rwcli_controller.is_netconf_agent_loaded = true;
  }

  /* Register the completion widget */
  rwcli_controller.w_comp = addzlefunction("rift-complete", rift_complete, 0);
  Keymap km = openkeymap("main");
  rwcli_controller.t_orig_tab_bind = keybind(km, "\t", &dummy);
  bindkey(km, "\t", refthingy(rwcli_controller.w_comp->first), NULL);

  /* Bind ? to generate help */
  rwcli_controller.w_gen_help = addzlefunction("rift-gen-help", 
                                      rift_generate_help, 0);
  bindkey(km, "?", refthingy(rwcli_controller.w_gen_help->first), NULL);

  /* Set the lookup hook */
  rw_lookup_fn = rift_lookup;

  /* Set the prompt hook */
  addprepromptfn(rift_prompt);

  return 0;
}

/* Called when the module is about to be unloaded. 
 * Cleanup used resources here. */

/**/
int
cleanup_(Module m)
{
  rw_lookup_fn = NULL;

  delprepromptfn(rift_prompt);
  deletezlefunction(rwcli_controller.w_comp);
  deletezlefunction(rwcli_controller.w_gen_help);
  rwcli_zsh_plugin_deinit();

  free(rwcli_controller.recv_buf);

  return setfeatureenables(m, &module_features, NULL);
}

/* Called when the module is unloaded. */

/**/
int
finish_(UNUSED(Module m))
{
    return 0;
}
