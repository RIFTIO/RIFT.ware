
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
 *
 */

#include <rw_tasklet_plugin.h>
#include <rwtasklet.h>
#include "rwtoytasklet-c.h"

struct rwtoytasklet_component_s {
  int foo;
};

RW_TYPE_DECL(rwtoytasklet_component);
RW_CF_TYPE_DEFINE("RW.Init RWTasklet Component Type", rwtoytasklet_component_ptr_t);

struct rwtoytasklet_instance_s {
  CFRuntimeBase _base;
  rwtasklet_info_ptr_t rwtasklet_info;
  rwmsg_srvchan_t *sc;
  rwmsg_clichan_t *cc;
  Echo_Client echo_cli;
  Echo_Service echo_srv;
  rwmsg_destination_t *dest;
  rwsched_CFRunLoopTimerRef stop_cftimer;
};

RW_TYPE_DECL(rwtoytasklet_instance);
RW_CF_TYPE_DEFINE("RW.Init RWTasklet Instance Type", rwtoytasklet_instance_ptr_t);

static void
rwtoytasklet_cftimer_callback(rwsched_CFRunLoopTimerRef timer, void *info);

static void
rwtoytasklet_ping_rsp(PingRsp* rsp,
                      rwmsg_request_t *req,
                      void *ud)
{
  rwtoytasklet_instance_ptr_t instance = (rwtoytasklet_instance_ptr_t) ud;
  UNUSED(req);
  UNUSED(rsp);

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwtoytasklet_instance_ptr_t);

  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
         RWTRACE_CATEGORY_RWTASKLET,
         "\n================================================================================\n\n"
         "RW.ToyTasklet[%d] -- RX ping response callback (%d)\n\n"
         "================================================================================\n",
         instance->rwtasklet_info->identity.rwtasklet_instance_id,
         rsp->sequence);

}

static void
rwtoytasklet_cftimer_callback(rwsched_CFRunLoopTimerRef timer, void *info)
{
  rwtoytasklet_instance_ptr_t instance;
  rw_status_t s;
  static int sequence = 1;

  // Validate input parameters
  instance = (rwtoytasklet_instance_ptr_t) info;
  RW_CF_TYPE_VALIDATE(instance, rwtoytasklet_instance_ptr_t);

  // Output a trace info message for the callback
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	       RWTRACE_CATEGORY_RWTASKLET,
	       "\n================================================================================\n\n"
	       "RW.ToyTasklet -- Entered cftimer callback\n\n"
	       "================================================================================\n");

  PingReq req;
  ping_req__init(&req);
  req.sequence = sequence++;
  rwmsg_closure_t clo = { { .pbrsp = (rwmsg_pbapi_cb) rwtoytasklet_ping_rsp }, .ud = (void*) instance };

  s = echo__send(&instance->echo_cli, instance->dest, &req, &clo, NULL);
  RW_ASSERT(s == RW_STATUS_SUCCESS);
}

RwTaskletPluginComponentHandle *
rwtoytasklet__component__component_init(RwTaskletPluginComponent *self)
{
  rwtoytasklet_component_ptr_t component;
  RwTaskletPluginComponentHandle *h_component;
  gpointer handle;

  // Register the RW.Init types
  RW_CF_TYPE_REGISTER(rwtoytasklet_component_ptr_t);
  RW_CF_TYPE_REGISTER(rwtoytasklet_instance_ptr_t);

  // Allocate a new rwtoytasklet_component structure
  component = RW_CF_TYPE_MALLOC0(sizeof(*component), rwtoytasklet_component_ptr_t);
  RW_CF_TYPE_VALIDATE(component, rwtoytasklet_component_ptr_t);

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_COMPONENT_HANDLE, 0);
  h_component = RW_TASKLET_PLUGIN_COMPONENT_HANDLE(handle);
  h_component->priv = (gpointer) component;

  // Return the handle to the rwtasklet component
  return h_component;
}

void
rwtoytasklet__component__component_deinit(RwTaskletPluginComponent *self,
					  RwTaskletPluginComponentHandle *h_component)
{
}

static RwTaskletPluginInstanceHandle *
rwtoytasklet__component__instance_alloc(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					struct rwtasklet_info_s * rwtasklet_info,
					RwTaskletPlugin_RWExecURL* instance_url)
{
  rwtoytasklet_component_ptr_t component;
  rwtoytasklet_instance_ptr_t instance;
  RwTaskletPluginInstanceHandle *h_instance;
  gpointer handle;

  // Validate input parameters
  component = (rwtoytasklet_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwtoytasklet_component_ptr_t);

  // Allocate a new rwtoytasklet_instance structure
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwtoytasklet_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwtoytasklet_instance_ptr_t);

  // Save the rwtasklet_info structure
  instance->rwtasklet_info = rwtasklet_info;

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_INSTANCE_HANDLE, 0);
  h_instance = RW_TASKLET_PLUGIN_INSTANCE_HANDLE(handle);
  h_instance->priv = (gpointer) instance;

  // Return the handle to the rwtasklet instance
  return h_instance;
}

void
rwtoytasklet__component__instance_free(RwTaskletPluginComponent *self,
				       RwTaskletPluginComponentHandle *h_component,
				       RwTaskletPluginInstanceHandle *h_instance)
{
}

void echo_send(Echo_Service *pingpong_srv, 
               const PingReq *input,
               void *self,
               PingRsp_Closure clo, 
               rwmsg_request_t *rwreq) {
  PingRsp rsp;
  rwtoytasklet_instance_ptr_t instance = (rwtoytasklet_instance_ptr_t) self;

  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	       RWTRACE_CATEGORY_RWTASKLET,
	       "\n================================================================================\n\n"
	       "RW.ToyTasklet[%d] -- Received Echo Request (%d) !\n\n"
	       "================================================================================\n",
         instance->rwtasklet_info->identity.rwtasklet_instance_id,
         input->sequence);

  ping_rsp__init(&rsp);       // The generated constructor for TestRsp
  rsp.sequence = input->sequence;
  clo(&rsp, rwreq);
  return;
}

static void
rwtoytasklet__component__instance_start(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					RwTaskletPluginInstanceHandle *h_instance)
{
  rwtoytasklet_component_ptr_t component;
  rwtoytasklet_instance_ptr_t instance;
  rw_status_t rwstatus;

  // Validate input parameters
  component = (rwtoytasklet_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwtoytasklet_component_ptr_t);
  instance = (rwtoytasklet_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwtoytasklet_instance_ptr_t);

  // The instance is started so print our RW.HelloWorld message
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	       RWTRACE_CATEGORY_RWTASKLET,
	       "\n================================================================================\n\n"
	       "RW.ToyTasklet -- Tasklet is started (%d) !\n\n"
	       "================================================================================\n",
         instance->rwtasklet_info->identity.rwtasklet_instance_id);

  // Read in our tasklet variable settings
  int echo_client;
  rwstatus = rwvcs_variable_evaluate_int(instance->rwtasklet_info->rwvcs,
                                         "$echo_client",
                                         &echo_client);
  if (rwstatus != RW_STATUS_SUCCESS)
    echo_client = -1;
  printf("echo_client (int) = %d\n", echo_client);
  

  RW_ASSERT(instance->rwtasklet_info->rwmsg_endpoint);

  if (echo_client == 0) {
    instance->sc = rwmsg_srvchan_create(instance->rwtasklet_info->rwmsg_endpoint);
    rwmsg_srvchan_bind_rwsched_cfrunloop(instance->sc, 
                                         instance->rwtasklet_info->rwsched_tasklet_info);
    ECHO__INITSERVER(&instance->echo_srv, echo_);
    rwmsg_srvchan_add_service(instance->sc, 
                              "/R/RW.ToyTasklet/1", 
                              (ProtobufCService *)&instance->echo_srv, 
                              instance);
  } else if (echo_client == 1) {
    // Instantiate client interface
    instance->dest = rwmsg_destination_create(instance->rwtasklet_info->rwmsg_endpoint,
                                              RWMSG_ADDRTYPE_UNICAST,
                                              "/R/RW.ToyTasklet/1");
    RW_ASSERT(instance->dest);

    // Create a client channel for registering
    instance->cc = rwmsg_clichan_create(instance->rwtasklet_info->rwmsg_endpoint);
    RW_ASSERT(instance->cc);

    ECHO__INITCLIENT(&instance->echo_cli);
    rwmsg_clichan_add_service(instance->cc, 
                              (ProtobufCService *)&instance->echo_cli);

    // Bind the channel to the run loop
    rwstatus = rwmsg_clichan_bind_rwsched_cfrunloop(instance->cc, 
                                                    instance->rwtasklet_info->rwsched_tasklet_info);
    RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);

    // Create a CFRunLoopTimer and add the timer to the current runloop
    // This time will perodically ping the echo server tasklet
    rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
    double timer_interval = 1000 * .001;
    rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(instance->rwtasklet_info->rwsched_tasklet_info);
    rwsched_CFRunLoopTimerRef cftimer;
    cf_context.info = instance;
    cftimer = rwsched_tasklet_CFRunLoopTimerCreate(instance->rwtasklet_info->rwsched_tasklet_info,
                  kCFAllocatorDefault,
                  CFAbsoluteTimeGetCurrent() + timer_interval,
                  timer_interval,
                  0,
                  0,
                  rwtoytasklet_cftimer_callback,
                  &cf_context);
    RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
    rwsched_tasklet_CFRunLoopAddTimer(
        instance->rwtasklet_info->rwsched_tasklet_info,
        runloop,
        cftimer,
        rwsched_instance_CFRunLoopGetMainMode(instance->rwtasklet_info->rwsched_instance));
  }
}

static void
rwtoytasklet__component__instance_stop(
    RwTaskletPluginComponent * self,
    RwTaskletPluginComponentHandle * h_component,
    RwTaskletPluginInstanceHandle * h_instance)
{
  rwtoytasklet_component_ptr_t component;
  rwtoytasklet_instance_ptr_t instance;

  // Validate input parameters
  component = (rwtoytasklet_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwtoytasklet_component_ptr_t);
  instance = (rwtoytasklet_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwtoytasklet_instance_ptr_t);

  // The instance is started so print our RW.HelloWorld message
  RWTRACE_WARN(instance->rwtasklet_info->rwtrace_instance,
      RWTRACE_CATEGORY_RWTASKLET,
      "RW.ToyTasklet[%d] -- Stop method undefined",
      instance->rwtasklet_info->identity.rwtasklet_instance_id);
}

static void
rwtoytasklet_c_extension_init(RwtoytaskletCExtension *plugin)
{
}

static void
rwtoytasklet_c_extension_class_init(RwtoytaskletCExtensionClass *klass)
{
}

static void
rwtoytasklet_c_extension_class_finalize(RwtoytaskletCExtensionClass *klass)
{
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
