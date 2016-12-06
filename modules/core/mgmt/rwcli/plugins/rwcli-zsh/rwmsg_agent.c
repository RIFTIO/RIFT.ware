
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
 * @file rwmsg_agent.c 
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 07/15/2015
 * @brief ZSH module to parse and execute rw.commands 
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#undef _GNU_SOURCE

#include <sys/prctl.h>


#include "rw_sys.h"
#include "rwcli_zsh.h"
#include "rwcli_agent.h"

#include "rwmsg_agent.mdh"

#define RWMSG_BROKER_ENABLE "RWMSG_BROKER_ENABLE"

#define RWMSG_BROKER_ENABLE_VALUE "1"

#define RWMSG_CLI_PATH "/R/RW.CLI/1"

#define RW_VM_ID "RWVM_INSTANCE_ID"

typedef struct rwcli_agent_stats_s
{
  uint64_t requests;
  uint64_t rpcs;
  uint64_t gets;
  uint64_t get_configs;
  uint64_t edit_configs;
  uint64_t requests_failed;
} rwcli_agent_stats_t;

extern rift_cmdargs_t rift_cmdargs;

static pid_t cli_agent_pid = -1;

// Buffer to store the packed message
static uint8_t* send_buffer = NULL;
static size_t send_buffer_len = 0;
static rwcli_agent_stats_t agent_stats;

static int msg_channel_init_server(rwcli_msg_channel_t* ch) {
  close(ch->in.fd.write);
  close(ch->out.fd.read);
  return 0;
}

/**
 * Receive message from controller. Received message is kept in inst->recv_buf.
 */
static rw_status_t agent_recv_from_controller(
                      rwcli_controller_t* inst,
                      rwcli_agent_msg_hdr_t *msg_hdr)
{
  ssize_t nread = read(inst->agent_channels[RWCLI_TRANSPORT_MODE_RWMSG].in.fd.read, 
                       msg_hdr, sizeof(rwcli_agent_msg_hdr_t));
  if (nread <= 0) {
    if (errno != EAGAIN && errno != EINTR) {
      RWTRACE_ERROR(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nSHELL: Error reading from CLI-AGENT: %s", strerror(errno));
    }
    return RW_STATUS_FAILURE;
  }
  if (msg_hdr->msg_len == 0) {
    // no payload
    return RW_STATUS_SUCCESS;
  }
  if (msg_hdr->msg_len > inst->recv_buf_len) {
    free(inst->recv_buf);
    inst->recv_buf = (uint8_t*)calloc(sizeof(uint8_t), msg_hdr->msg_len);
    inst->recv_buf_len = msg_hdr->msg_len;
  }

  // Read until we received the complete message
  size_t nr = 0;
  size_t remaining = msg_hdr->msg_len;
  nread = 0;
  do {
    nr = read(inst->agent_channels[RWCLI_TRANSPORT_MODE_RWMSG].in.fd.read, 
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

/**
 * Sends the message to controller.
 */
static rw_status_t agent_send_to_controller(
                      rwcli_controller_t* inst,
                      uint8_t* msg_buf,
                      unsigned msg_len)
{
  int nw = 0;
  nw = write(inst->agent_channels[RWCLI_TRANSPORT_MODE_RWMSG].out.fd.write, 
            (const void*)(&msg_len), sizeof(unsigned));
  if (nw <= 0) {
    RWTRACE_ERROR(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nSHELL: Error writing to CLI-AGENT %s\n", strerror(errno));
    return RW_STATUS_FAILURE;
  }

  if (msg_buf) {
    nw = write(inst->agent_channels[RWCLI_TRANSPORT_MODE_RWMSG].out.fd.write, 
             msg_buf, msg_len);
    if (nw <= 0) {
      RWTRACE_ERROR(inst->trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nSHELL: Error writing to CLI-AGENT %s\n", strerror(errno));
      return RW_STATUS_FAILURE;
    }
  }
  return  RW_STATUS_SUCCESS;
}

/**
 * Updates the agent statistics based on the received request.
 */
static void agent_update_stats(NetconfReq* req)
{
  if (req->has_operation) {
    agent_stats.requests++;
    switch(req->operation) {
      case nc_get: 
        agent_stats.gets++; 
        break;
      case nc_get_config: 
        agent_stats.get_configs++;
        break;
      case nc_edit_config:
        agent_stats.edit_configs++;
        break;
      case nc_default:
      case nc_rpc_exec:
        agent_stats.rpcs++;
        break;
      default:
        break;
    }
  }
}


/* Module Features. We don't have any. */
static struct features module_features = {
    NULL, 0,
    NULL, 0,
    NULL, 0,
    NULL, 0,
    0
};

static struct rwtasklet_info_s * create_tasklet_info(
    const char * tasklet_instance_name,
    uint32_t tasklet_instance_id)
{
  struct rwtasklet_info_s * info;
  int broker_id = 1;

  // lookup broker id
  const char* env_str = rw_getenv(RW_VM_ID);
  if (env_str) {
    broker_id = atoi(env_str);
  }

  info = (struct rwtasklet_info_s *)malloc(sizeof(struct rwtasklet_info_s));
  RW_ASSERT(info);
  bzero(info, sizeof(struct rwtasklet_info_s));

  info->rwsched_instance = rwsched_instance_new();
  info->rwsched_instance->rwlog_instance = rwlog_init("RW.Sched");
  info->rwsched_tasklet_info = rwsched_tasklet_new(info->rwsched_instance);
  info->rwtrace_instance = rwtrace_init();
  info->rwvx = NULL;
  info->rwvcs = NULL;
  info->identity.rwtasklet_instance_id = tasklet_instance_id;
  info->identity.rwtasklet_name = strdup(tasklet_instance_name);
  info->rwlog_instance = NULL;

  info->rwmsg_endpoint = rwmsg_endpoint_create(
      1,
      tasklet_instance_id,
      broker_id, // ATTN: this might be broken in single-broker mode
      info->rwsched_instance,
      info->rwsched_tasklet_info,
      info->rwtrace_instance,
      NULL);

  return info;
}

rwcli_component_ptr_t
rwcli_component_init(void)
{
  rwcli_component_ptr_t component;

  // Allocate a new rwcli_component structure
  component = RW_CF_TYPE_MALLOC0(sizeof(*component), rwcli_component_ptr_t);
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);

  // Return the allocated component
  return component;
}

rwcli_instance_ptr_t
rwcli_instance_alloc(rwcli_component_ptr_t component,
		     struct rwtasklet_info_s * rwtasklet_info,
         RwTaskletPlugin_RWExecURL *instance_url)
{
  (void)instance_url;
  rwcli_instance_ptr_t instance;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);

  // Allocate a new rwcli_instance structure
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwcli_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);
  instance->component = component;

  // Save the rwtasklet_info structure
  instance->rwtasklet_info = rwtasklet_info;

  // Return the allocated instance
  return instance;
}


/* Callback fn receiving log notification from messaging.
 * Print the logs */
static rw_status_t
rwlogd_register_client(Rwlogd_Service *service,
                          const RwlogdRegisterReq *input,
                          void *usercontext,
                          RwlogdRegisterReq_Closure closure,
                          void *closure_data)
{
   // Create the shared memory here .. 
   //clo(NULL, closure_data);
  if (input && input->my_key && input->my_key[0]) {
    printf ("%s\n", input->my_key);
    fflush(stdout);
  }
   return RW_STATUS_SUCCESS;

}

static rw_status_t
rwtasklet_log_endpoint_init(rwcli_instance_ptr_t instance)
{
  rw_status_t rwstatus;
  // Instantiate server interface

  //Initialize the protobuf service
  RWLOGD__INITSERVER(&instance->rwlogd_srv, rwlogd_);

  RW_ASSERT(instance->rwtasklet_info->rwmsg_endpoint);
  // Create a server channel that tasklet uses to recieve messages from clients
  instance->sc = rwmsg_srvchan_create(instance->rwtasklet_info->rwmsg_endpoint);
  RW_ASSERT(instance->sc);

  // Bind a single service for tasklet namespace
  rwstatus = rwmsg_srvchan_add_service(instance->sc,
                                       RWMSG_CLI_PATH,
                                       (ProtobufCService *)&instance->rwlogd_srv,
                                       instance);
  RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);
  // Bind this srvchan to rwsched's taskletized cfrunloop
  // This will result in events executing in the cfrunloop context
  // rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.rwsched);
  rwstatus = rwmsg_srvchan_bind_rwsched_cfrunloop(instance->sc,
                                                  instance->rwtasklet_info->rwsched_tasklet_info);
  RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);
  return rwstatus;
}

/**
 * Handler for get-transport-status message. Sends the formatted string to the
 * controller.
 */
static void handle_get_transport_status(rwcli_instance_ptr_t instance) 
{
  rwcli_internal_rsp_t* rsp;
  rw_status_t status;
  char *ret_str = NULL;
  int len = 0;
  NetconfReq nc_req;
  XmlBlob blob;
  rwmsg_request_t* rwreq;

  // Send a 'show uptime' command to see if RW.MgmtAgent is reachable or not
  // The rwmsg_clichan_connection_status is not reliable to get the connection
  // status. 
  xml_blob__init(&blob);
  netconf_req__init(&nc_req);
  blob.xml_blob = (char*)"<?xml version= \"1.0\" encoding=\"UTF-8\"?> \n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\"><uptime/></data>";
  nc_req.xml_blob = &blob; 
  nc_req.has_operation = 1;
  nc_req.operation = nc_get;

  status = rw_uagent__netconf_request_b(&instance->uagent.rwuacli, 
                    instance->uagent.dest, &nc_req, &rwreq);

  len = asprintf(&ret_str, 
      "RW.MSG Transport:\n"
      "  Connection Status: %s\n"
      "  Statistics:\n"
      "    Requests        : %lu\n"
      "    RPCs            : %lu\n"
      "    Gets            : %lu\n"
      "    Get-Configs     : %lu\n"
      "    Edit-Configs    : %lu\n"
      "    Failed Requests : %lu\n",
      (status == RW_STATUS_SUCCESS)? "Connected" : "Not Connected",
      agent_stats.requests,
      agent_stats.rpcs,
      agent_stats.gets,
      agent_stats.get_configs,
      agent_stats.edit_configs,
      agent_stats.requests_failed
      );
  RW_ASSERT(len > 0);

  rsp = (rwcli_internal_rsp_t*)alloca(sizeof(rwcli_internal_rsp_t) + len);
  rsp->status = RW_STATUS_SUCCESS;
  rsp->str_data_len = len;
  memcpy(rsp->str_data, ret_str, len); 

  len += sizeof(rwcli_internal_rsp_t);
  agent_send_to_controller(&rwcli_controller, (uint8_t*)rsp, len);

  free(ret_str);
}

/**
 * Handler for internal message from the agent.
 */
static void handle_internal_message(
              rwcli_instance_ptr_t instance,
              rwcli_agent_msg_hdr_t* msg_hdr)
{
  switch(msg_hdr->msg_type) {
    case RWCLI_MSG_TYPE_GET_TRANSPORT_STATUS: {
      handle_get_transport_status(instance);
      break;
    }
    default:
      RW_ASSERT_NOT_REACHED();
  }
}


/**
 * Handles messages from the RW.CLI controller. 
 */
static void handle_cli_input(
    rwsched_CFSocketRef s,
    CFSocketCallBackType type,
    CFDataRef address,
    const void *data,
    void *info)
{
  rwcli_instance_ptr_t instance = (rwcli_instance_ptr_t)info;
  rwcli_agent_msg_hdr_t msg_hdr;
  NetconfReq* nc_req = NULL;
  NetconfRsp* nc_rsp = NULL;
  rw_status_t rwstatus;
  rwmsg_request_t* rwreq;

  if(1) {
    unsigned msg_len = 0;
    rwstatus = agent_recv_from_controller(&rwcli_controller, &msg_hdr);
    if (rwstatus != RW_STATUS_SUCCESS) {
      goto _done;
    }

    RWTRACE_DEBUG(rwcli_controller.trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "CLI-AGENT: Received %d bytes from controller", msg_hdr.msg_len);

    if (msg_hdr.msg_type != RWCLI_MSG_TYPE_RW_NETCONF) {
      handle_internal_message(instance, &msg_hdr);
      return;
    }

    // This is awfully inefficient conversion from packed to unpacked
    // then again to packed. To reuse the existing RPC, it is done this way
    // TODO use the rwmsg clichan APIs to directly send the buffer

    nc_req = netconf_req__unpack(NULL, msg_hdr.msg_len, rwcli_controller.recv_buf);
    RW_ASSERT(nc_req);


    /* invoke the rpc */
    rwstatus = rw_uagent__netconf_request_b(&instance->uagent.rwuacli, 
                    instance->uagent.dest, nc_req, &rwreq);
    if (rwstatus == RW_STATUS_SUCCESS) {
      size_t rsp_len = 0;
      rwstatus = rwmsg_request_get_response(rwreq, (const void**)&nc_rsp);
      if (rwstatus != RW_STATUS_SUCCESS) {
        RWTRACE_ERROR(rwcli_controller.trace_ctxt, RWTRACE_CATEGORY_RWCLI,
            "CLI-AGENT: Failed to retrieve the response");
        agent_send_to_controller(&rwcli_controller, NULL, 0);
        netconf_req__free_unpacked(NULL, nc_req);
        return;
      }

      rsp_len = netconf_rsp__get_packed_size(NULL, nc_rsp);
      if (rsp_len > send_buffer_len) {
        free(send_buffer);
        send_buffer_len = rsp_len;
        send_buffer = (uint8_t*)calloc(sizeof(char), send_buffer_len);
      }

      msg_len = netconf_rsp__pack(NULL, nc_rsp, send_buffer);

      RWTRACE_DEBUG(rwcli_controller.trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nCLI-AGENT: Relaying response received len %d", msg_len);

      agent_update_stats(nc_req);
      agent_send_to_controller(&rwcli_controller, send_buffer, msg_len);
    } else {
      agent_stats.requests_failed++;
      if (rwstatus == RW_STATUS_NOTCONNECTED) {
        fprintf(stderr, "ERROR: Not connected to RW.MgmtAgent\n");
      } else {
        fprintf(stderr, "ERROR: Failed to send message to RW.MgmtAgent\n");
      }
      fflush(stderr);

      agent_send_to_controller(&rwcli_controller, NULL, 0);
    }

    netconf_req__free_unpacked(NULL, nc_req);
  }
  return;
_done:
  free(send_buffer);
  _exit(0);
}

static void setup_cfsource(rwcli_instance_ptr_t instance)
{
  CFSocketContext cf_context;
  rwsched_CFSocketRef cfsocket;
  rwsched_CFRunLoopSourceRef cfsource;

  bzero(&cf_context, sizeof(cf_context));
  cf_context.info = (void *)instance;

  cfsocket = rwsched_tasklet_CFSocketCreateWithNative(
      instance->rwtasklet_info->rwsched_tasklet_info,
      kCFAllocatorSystemDefault,
      rwcli_controller.agent_channels[RWCLI_TRANSPORT_MODE_RWMSG].in.fd.read,
      kCFSocketReadCallBack,
      handle_cli_input,
      &cf_context);
  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);

  rwsched_tasklet_CFSocketSetSocketFlags(
      instance->rwtasklet_info->rwsched_tasklet_info,
      cfsocket,
      kCFSocketAutomaticallyReenableReadCallBack);

  cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(
      instance->rwtasklet_info->rwsched_tasklet_info,
      kCFAllocatorSystemDefault,
      cfsocket,
      0);
  RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);

  rwsched_tasklet_CFRunLoopAddSource(
      instance->rwtasklet_info->rwsched_tasklet_info,
      rwsched_tasklet_CFRunLoopGetCurrent(instance->rwtasklet_info->rwsched_tasklet_info),
      cfsource,
      rwsched_instance_CFRunLoopGetMainMode(instance->rwtasklet_info->rwsched_instance));

}


static void cli_agent_subprocess()
{
  // Register the RW.Init types
  RW_CF_TYPE_REGISTER(rwcli_component_ptr_t);
  RW_CF_TYPE_REGISTER(rwcli_instance_ptr_t);

  rwcli_component_ptr_t component = rwcli_component_init(); 
  RW_ASSERT(component);

  struct rwtasklet_info_s *tinfo = create_tasklet_info("RW.CLI", 1);

  rwcli_instance_ptr_t instance = rwcli_instance_alloc(component, tinfo, NULL);
  RW_ASSERT(instance);
  
  RW_CF_TYPE_REGISTER(rwuagent_client_info_ptr_t);
  RW_CF_TYPE_ASSIGN(&instance->uagent, rwuagent_client_info_ptr_t);
  instance->uagent.rwtasklet_info = instance->rwtasklet_info;
  instance->uagent.user_context = instance;
  rwtasklet_register_uagent_client(&instance->uagent);
  rwtasklet_log_endpoint_init(instance);

  memset(&agent_stats, 0, sizeof(agent_stats));

  // Inititalize the send buffer
  send_buffer = (uint8_t*)calloc(sizeof(char), RW_CLI_MAX_BUF);
  send_buffer_len = RW_CLI_MAX_BUF;

  // Setup the pipe irrespective of the mode, used when transports are switched
  // at the CLI
  msg_channel_init_server(&rwcli_controller.agent_channels[RWCLI_TRANSPORT_MODE_RWMSG]);
  setup_cfsource(instance);

  while(1) {
    rwsched_instance_CFRunLoopRunInMode(instance->rwtasklet_info->rwsched_instance, instance->rwtasklet_info->rwsched_instance->main_cfrunloop_mode, 1000.00, false);
  }

  // TODO cleanup rwmsg
  _exit(0);
}

static void launch_cli_agent() 
{
  int ret = 0;
  const int no_overwrite = 0;
  // The Msg channel should have been created by this time

  // Set required environment variables for the broker to communicate
  setenv(RWMSG_BROKER_ENABLE, RWMSG_BROKER_ENABLE_VALUE, no_overwrite);

  cli_agent_pid = fork();
  switch(cli_agent_pid) {
    case 0:
      // if the parent exits the child also exits
      ret = prctl(PR_SET_PDEATHSIG, SIGTERM);
      RW_ASSERT(ret == 0);

      signal(SIGTERM, SIG_DFL);

      cli_agent_subprocess();
      break;
    case -1:
      RWTRACE_CRIT(rwcli_controller.trace_ctxt, RWTRACE_CATEGORY_RWCLI,
          "\nError: Unable to fork CLI Agent: %s", strerror(errno));
      break;
    default: {
    }
  }
}


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

/* Called after the features are enabled when the module is loaded.
 * Module initialization code goes here. */

/**/
int
boot_(Module m)
{
  launch_cli_agent(); 
  RWTRACE_INFO(rwcli_controller.trace_ctxt, RWTRACE_CATEGORY_RWCLI,
      "MAIN: CLI-AGENT subprocess launched");

  return 0;
}

/* Called when the module is about to be unloaded. 
 * Cleanup used resources here. */

/**/
int
cleanup_(Module m)
{
  return setfeatureenables(m, &module_features, NULL);
}

/* Called when the module is unloaded. */

/**/
int
finish_(UNUSED(Module m))
{
    return 0;
}

