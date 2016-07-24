
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#undef FOO

#define _GNU_SOURCE
#include "stdio.h"
#include "rwcli.h"
#include "rwcli.hpp"
#include "rwvcs_rwzk.h"

char g_cmdargs_str[1024];
rwcli_component_ptr_t
rwcli_component_init(void)
{
  rwcli_component_ptr_t component;

  // Allocate a new rwcli_component structure
  component = RW_CF_TYPE_MALLOC0(sizeof(*component), rwcli_component_ptr_t);
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);

  // Wire in xsdcli
  xsdcli_component_init();

  // Return the allocated component
  return component;
}

void
rwcli_component_deinit(rwcli_component_ptr_t component)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);
}

rwcli_instance_ptr_t
rwcli_instance_alloc(rwcli_component_ptr_t component,
		     struct rwtasklet_info_s * rwtasklet_info,
		     RwTaskletPlugin_RWExecURL *instance_url)
{
  rwcli_instance_ptr_t instance;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);
  RW_ASSERT(instance_url);

  // Allocate a new rwcli_instance structure
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwcli_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);
  instance->component = component;

  // Save the rwtasklet_info structure
  instance->rwtasklet_info = rwtasklet_info;

  // Return the allocated instance
  return instance;
}

void
rwcli_instance_free(rwcli_component_ptr_t component,
		    rwcli_instance_ptr_t instance)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);
}

static void
rwcli_cftimer_callback(rwsched_CFRunLoopTimerRef timer, void *info);

static void
rwcli_cftimer_callback(rwsched_CFRunLoopTimerRef timer, void *info)
{
  rwcli_instance_ptr_t instance;
  int argc = 0;
  char **argv = NULL;
  gboolean ret;
  char *instance_name = NULL;

  // Validate input parameters
  RW_ASSERT(timer);
  instance = (rwcli_instance_ptr_t) info;
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);

  // Log a trace message for entering the cftimer callback routine
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	             RWTRACE_CATEGORY_RWTASKLET,
	             "RW.Cli -- Entered cftimer callback");
  rwtasklet_info_ptr_t ti = instance->rwtasklet_info;
  if (ti && ti->rwvcs)
  {
    instance_name  = to_instance_name(ti->identity.rwtasklet_name,
                                      ti->identity.rwtasklet_instance_id);
    rw_status_t status = rwvcs_rwzk_update_state(ti->rwvcs, 
                                                 instance_name, 
                                                 RW_BASE_STATE_TYPE_RUNNING);
    RW_ASSERT(RW_STATUS_SUCCESS == status);
    free(instance_name);
  }
  // HACK: This eventually need to come from management agent
  ret = g_shell_parse_argv(g_cmdargs_str, &argc, &argv, NULL);
  RW_ASSERT(ret == TRUE);

  // cmdline args for the module must be specified in the manifest
  RW_ASSERT(argc > 0);
  RW_ASSERT(argv);

  // Wire in xsdcli
  xsdcli_instance_init(instance->component, instance, argc, argv);

  // Wire in xsdcli
  xsdcli_instance_start(instance->component, instance);

  /* TODO: Can this be cleaned up */
  // g_strfreev(argv);
}


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
  }
   return RW_STATUS_SUCCESS;

}
static rw_status_t
rwtasklet_log_endpoint_init(rwcli_instance_ptr_t instance)
{
  rw_status_t rwstatus;
  // Instantiate server interface
  char *my_path;
  int r = asprintf(&my_path,
                   "/R/%s/%d",
                   instance->rwtasklet_info->identity.rwtasklet_name,
                   instance->rwtasklet_info->identity.rwtasklet_instance_id);
  RW_ASSERT(r != -1);

  //Initialize the protobuf service
  RWLOGD__INITSERVER(&instance->rwlogd_srv, rwlogd_);

  RW_ASSERT(instance->rwtasklet_info->rwmsg_endpoint);
  // Create a server channel that tasklet uses to recieve messages from clients
  instance->sc = rwmsg_srvchan_create(instance->rwtasklet_info->rwmsg_endpoint);
  RW_ASSERT(instance->sc);

  // Bind a single service for tasklet namespace
  rwstatus = rwmsg_srvchan_add_service(instance->sc,
                                       my_path,
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

void
rwcli_instance_start(rwcli_component_ptr_t component,
		     rwcli_instance_ptr_t instance)
{
  rw_status_t status;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);

  // The instance is started so print our RW.Cli message
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	             RWTRACE_CATEGORY_RWTASKLET,
	             "RW.Cli -- Tasklet is started!");

  // HACK: This eventually need to come from management agent
  status = rwvcs_variable_evaluate_str(instance->rwtasklet_info->rwvcs,
                                       "$cmdargs_str",
                                       g_cmdargs_str,
                                       sizeof(g_cmdargs_str));
  RW_ASSERT(RW_STATUS_SUCCESS == status);
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "cmdargs_str: %s\n", g_cmdargs_str);

  // Create a CFRunLoopTimer and add the timer to the current runloop
  // This timer will start the CLI
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  double timer_interval = 1000 * .001;
  rwsched_CFRunLoopTimerRef cftimer;
  rwsched_CFRunLoopRef runloop;

  runloop = rwsched_tasklet_CFRunLoopGetCurrent(instance->rwtasklet_info->rwsched_tasklet_info);
  cf_context.info = instance;
  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(instance->rwtasklet_info->rwsched_tasklet_info,
					 kCFAllocatorDefault,
					 CFAbsoluteTimeGetCurrent() + timer_interval,
					 0,
					 0,
					 0,
					 rwcli_cftimer_callback,
					 &cf_context);
  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
  rwsched_tasklet_CFRunLoopAddTimer(instance->rwtasklet_info->rwsched_tasklet_info,
                            runloop, 
                            cftimer, 
                            instance->rwtasklet_info->rwsched_instance->main_cfrunloop_mode);

  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	             RWTRACE_CATEGORY_RWTASKLET,
	             "RW.Cli -- Created RW.Sched cftimer for interval = %lf!",
	             timer_interval);
  
  RW_CF_TYPE_REGISTER(rwuagent_client_info_ptr_t);
  RW_CF_TYPE_ASSIGN(&instance->uagent, rwuagent_client_info_ptr_t);
  instance->uagent.rwtasklet_info = instance->rwtasklet_info;
  instance->uagent.user_context = instance;
  rwtasklet_register_uagent_client(&instance->uagent);
  rwtasklet_log_endpoint_init(instance);
}
