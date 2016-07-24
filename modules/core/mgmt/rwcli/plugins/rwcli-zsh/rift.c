
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rift.cpp
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 07/15/2015
 * @brief ZSH module to parse and execute rw.commands 
 */

#include "rwcli_zsh.h"
#include "rwcli_agent.h"

#include <poll.h>
#include <openssl/sha.h>

#ifdef alloca
# undef alloca
#endif
#ifdef UNUSED
# undef UNUSED
#endif

/* Required for Widget and other definitions from ZLE */
#include "zle.mdh"

/* These two are generated from the rift.mdd file by mkmakemod.sh */
#include "rift.mdh"
#include "rift.pro"

#define RW_DEFAULT_MANIFEST "cli_rwfpath.xml"

extern rift_cmdargs_t rift_cmdargs;

static int rift_exec(char *nam, char **argv, Options ops, int func);

/* Before executing the command, the rift plugin returns this builtin, so that
 * we get a callback to execute the command */
static struct builtin rw_command_bn =
    BUILTIN("rw", BINF_RW_CMD, rift_exec, 0, -1, -1, NULL, NULL);

/* The widget used for tab completion */
static Widget w_comp = NULL;

/* Widget used for generating help */
static Widget w_gen_help = NULL;


/* Original thingy used for tab completion */
static Thingy t_orig_tab_bind = NULL;

static NetconfRsp* rw_resp = NULL;
static uint8_t* recv_buf = NULL;
static size_t recv_buf_len = 0;

rwcli_msg_channel_t rwcli_agent_ch;

rwtrace_ctx_t* rwcli_trace;

/*
 * Initializes the message channel between agent and the CLI.
 */
static void msg_channel_init(rwcli_msg_channel_t* ch)
{
  ch->in.fd.read = -1;
  ch->in.fd.write = -1;
  ch->out.fd.read = -1;
  ch->out.fd.write = -1;
}

/*
 * Creates the pipes to communicate with the agent
 */
static int msg_channel_create(rwcli_msg_channel_t* ch) {
  int ret = pipe(ch->in.fds);
  if (ret != 0) {
    RWTRACE_CRIT(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
        "Error creating IN pipe: %s", strerror(errno));
    return -1;
  }

  ret = pipe(ch->out.fds);
  if (ret != 0) {
    RWTRACE_CRIT(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
        "Error creating OUT pipe: %s", strerror(errno));
    return -1;
  }
  return 0;
}

/*
 * Recevie a message from the agent.
 *
 * In the message the first 4bytes provide the message length. The rest is the
 * NetconfResp protobuf.
 */
static rw_status_t recv_msg_from_agent(unsigned *msg_len)
{
  ssize_t nread = read(rwcli_agent_ch.out.fd.read, msg_len, sizeof(unsigned));
  if (nread <= 0) {
    if (errno != EINTR) {
      RWTRACE_ERROR(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
          "\nSHELL: Error reading from CLI-AGENT: %s", strerror(errno));
    }
    return RW_STATUS_FAILURE;
  }
  if (*msg_len == 0) {
    // no reply
    return RW_STATUS_FAILURE;
  }
  if (*msg_len > recv_buf_len) {
    free(recv_buf);
    recv_buf = (uint8_t*)calloc(sizeof(uint8_t), *msg_len);
    recv_buf_len = *msg_len;
  }

  // Read until we received the complete message
  size_t nr = 0;
  size_t remaining = *msg_len;
  nread = 0;
  do {
    nr = read(rwcli_agent_ch.out.fd.read, recv_buf + nread, remaining);
    if (recv <= 0) {
      RWTRACE_ERROR(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
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
static void consume_unread_agent_messages()
{
  unsigned msg_len = 0;
  rw_status_t status = RW_STATUS_SUCCESS;
  int ret = 0;
  struct pollfd fds[] = {
    { rwcli_agent_ch.out.fd.read, POLLIN, 0}
  };

  // Consume the messages until there is none to read
  while (1) {
    // Check if read is set on the fd and return immediately
    ret = poll(fds, 1, 0);
    if (ret == 0) {
      // No events
      return;
    }
    
    status = recv_msg_from_agent(&msg_len);
    if (status != RW_STATUS_SUCCESS) {
      return;
    }
  }
}

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
      execzlefunc(t_orig_tab_bind, zlenoargs, 0);
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
static int rift_exec(char *nam, char **args, UNUSED(Options ops), UNUSED(int func))
{
  char* buf[RW_CLI_MAX_ARGS] = {0};
  int len = 0;
  rw_resp = NULL;

  buf[0] = nam;
  while (*args) {
    buf[++len] = *args;
    args++;
    RW_ASSERT_MESSAGE (len < sizeof(buf)/sizeof(buf[0]),
        "Command arguments more than %d configured", RW_CLI_MAX_ARGS);
  }

  rwcli_exec_command(len + 1, (const char* const*)buf);
  fflush(stdout);

  if (rw_resp) {
    netconf_rsp__free_unpacked(NULL, rw_resp);
    rw_resp = NULL;
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

/* Module Features. We don't have any. */
static struct features module_features = {
    NULL, 0,
    NULL, 0,
    NULL, 0,
    NULL, 0,
    0
};

/* The empty comments have special meaning. the ZSH generators use them for
 * exporting the methods */

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

/**
 * This method will be invoked whene the rw.cli executes a command and 
 * requires a transport to send the message.
 */
static rw_status_t messaging_hook(NetconfReq *req, NetconfRsp **rsp)
{
  // Send message to the CLI-AGENT and wait for a message back
  unsigned msg_len = netconf_req__get_packed_size(NULL, req);
  uint8_t msg_buf[msg_len];
  rw_status_t status;
  
  netconf_req__pack(NULL, req, msg_buf);

  if (rwcli_agent_ch.in.fd.write == -1) {
    RWTRACE_CRIT(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
        "Messaging not initialized, failed to execute the command");
    return RW_STATUS_FAILURE;
  }

  // Consume any prvious unread messages on the stream
  consume_unread_agent_messages();

  // TODO handle EPIPE
  write(rwcli_agent_ch.in.fd.write, (const void*)(&msg_len), sizeof(unsigned));
  write(rwcli_agent_ch.in.fd.write, msg_buf, msg_len);

  RWTRACE_DEBUG(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
      "\nSHELL: sent %d bytes to CLI-AGENT", msg_len);

  status = recv_msg_from_agent(&msg_len);
  if (status != RW_STATUS_SUCCESS) {
    *rsp = NULL;
    return status;
  }

  RWTRACE_DEBUG(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
      "\nSHELL: received %u bytes from CLI-AGENT msglen\n", msg_len);

  rw_resp = netconf_rsp__unpack(NULL, msg_len, recv_buf);
  if (rw_resp == NULL) {
    RWTRACE_ERROR(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
          "\nReceived message unpack failed\n");
    *rsp = NULL;
    return RW_STATUS_FAILURE;
  }

  *rsp = rw_resp;

  return RW_STATUS_SUCCESS;
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

void rift_init_trace()
{
  rwcli_trace = rwtrace_init();
  RW_ASSERT(rwcli_trace);

  if (rift_cmdargs.trace_level == -1) {
    // trace level not set, using ERROR as default
    rift_cmdargs.trace_level = RWTRACE_SEVERITY_ERROR;
  }

  rwtrace_ctx_category_destination_set(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
                RWTRACE_DESTINATION_CONSOLE);
  rwtrace_ctx_category_severity_set(rwcli_trace, RWTRACE_CATEGORY_RWCLI,
                (rwtrace_category_t)rift_cmdargs.trace_level);
}


/* Called after the features are enabled when the module is loaded.
 * Module initialization code goes here. */

/**/
int
boot_(Module m)
{
  char* dummy = NULL;

  // Initialize the trace module and set appropriate severity
  rift_init_trace();

  recv_buf = (uint8_t*)calloc(sizeof(uint8_t), RW_CLI_MAX_BUF);
  recv_buf_len = RW_CLI_MAX_BUF;
  
  // Decide the RW.CLI communication mode with RW.MgmtAgent
  if (rift_cmdargs.use_netconf == -1) {
    const char* agent_mode = rw_getenv("RWMGMT_AGENT_MODE");
    if (agent_mode && strcmp(agent_mode, "XML") == 0) {
      // If the agent mode is XML only RW.MSG mode is possible. Override the
      // command line option for netconf and opt for RW.MSG mode.
      rift_cmdargs.use_netconf = 0;
    } else {
      // Transport mode not chosen in command line and not XML mode. Choose
      // Netconf mode by default.
      rift_cmdargs.use_netconf = 1;
    }
  }

  // Authenticate in case of RW.MSG mode
  rwcli_transport_mode_t transport_mode =  RWCLI_TRANSPORT_MODE_NETCONF;
  rwcli_user_mode_t user_mode = RWCLI_USER_MODE_NONE;
  if (!rift_cmdargs.use_netconf) {
    user_mode = rift_authenticate(&rift_cmdargs);
    if (user_mode == RWCLI_USER_MODE_INVALID) {
      printf("\nERROR: Invalid username/password\n");
      exit(-1);
    }
    transport_mode = RWCLI_TRANSPORT_MODE_RWMSG;
  }

  rwcli_zsh_plugin_init(transport_mode, user_mode);
  rwcli_set_messaging_hook(messaging_hook);
  rwcli_set_history_hook(history_hook);

  // To support background processes and job control, there should
  // be multiple channels available
  msg_channel_init(&rwcli_agent_ch);
  msg_channel_create(&rwcli_agent_ch);

  // Always load the rwmsg_agent module, If netconf is enabled then 
  // this module will only receive logging notifications
  if (load_module("zsh/rwmsg_agent", NULL, 0) != 0) {
    printf("\nCRITICAL: Loading the messaging agent module failed\n");
    fflush(stdout);
    return -1;
  }

  // Load the agent that is required
  if (rift_cmdargs.use_netconf) {
    if (load_module("zsh/rwnetconf_agent", NULL, 0) != 0) {
      printf("\nCRITICAL: Loading the netconf agent module failed\n");
      fflush(stdout);
      return -1;
    }
  }
  /* Register the completion widget */
  w_comp = addzlefunction("rift-complete", rift_complete, 0);
  Keymap km = openkeymap("main");
  t_orig_tab_bind = keybind(km, "\t", &dummy);
  bindkey(km, "\t", refthingy(w_comp->first), NULL);

  /* Bind ? to generate help */
  w_gen_help = addzlefunction("rift-gen-help", rift_generate_help, 0);
  bindkey(km, "?", refthingy(w_gen_help->first), NULL);

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
  deletezlefunction(w_comp);
  deletezlefunction(w_gen_help);
  rwcli_zsh_plugin_deinit();

  free(recv_buf);

  return setfeatureenables(m, &module_features, NULL);
}

/* Called when the module is unloaded. */

/**/
int
finish_(UNUSED(Module m))
{
    return 0;
}
