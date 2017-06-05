
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include "rwdtsperf-c.h"

RW_CF_TYPE_DEFINE("RW.Dtsperf RWTasklet Component Type", rwdtsperf_component_ptr_t);

RW_CF_TYPE_DEFINE("RW.Dtsperf RWTasklet Instance Type", rwdtsperf_instance_ptr_t);

RwTaskletPluginComponentHandle *
rwdtsperf__component__component_init(RwTaskletPluginComponent *self)
{
  rwdtsperf_component_ptr_t component;
  RwTaskletPluginComponentHandle *h_component;
  gpointer handle;

  // Register the RW.Init types
  RW_CF_TYPE_REGISTER(rwdtsperf_component_ptr_t);
  RW_CF_TYPE_REGISTER(rwdtsperf_instance_ptr_t);

  // Allocate a new rwdtsperf_component structure
  component = rwdtsperf_component_init();
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_COMPONENT_HANDLE, 0);
  h_component = RW_TASKLET_PLUGIN_COMPONENT_HANDLE(handle);
  h_component->priv = (gpointer) component;

  // Return the handle to the rwtasklet component
  return h_component;
}

void
rwdtsperf__component__component_deinit(RwTaskletPluginComponent *self,
					  RwTaskletPluginComponentHandle *h_component)
{
  rwdtsperf_component_ptr_t component;

  // Validate input parameters
  component = (rwdtsperf_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);

  rwdtsperf_component_deinit (component);
}

static RwTaskletPluginInstanceHandle *
rwdtsperf__component__instance_alloc(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					struct rwtasklet_info_s * rwtasklet_info,
					RwTaskletPlugin_RWExecURL* instance_url)
{
  rwdtsperf_component_ptr_t component;
  rwdtsperf_instance_ptr_t instance;
  RwTaskletPluginInstanceHandle *h_instance;
  gpointer handle;

  // Validate input parameters
  component = (rwdtsperf_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);

  // Allocate a new rwdtsperf_instance structure
  instance = rwdtsperf_instance_alloc (component, rwtasklet_info,instance_url);
  RW_CF_TYPE_VALIDATE(instance, rwdtsperf_instance_ptr_t);
  instance->component = component;

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_INSTANCE_HANDLE, 0);
  h_instance = RW_TASKLET_PLUGIN_INSTANCE_HANDLE(handle);
  h_instance->priv = (gpointer) instance;

  // Return the handle to the rwtasklet instance
  return h_instance;
}

void
rwdtsperf__component__instance_free(RwTaskletPluginComponent *self,
				       RwTaskletPluginComponentHandle *h_component,
				       RwTaskletPluginInstanceHandle *h_instance)
{
  rwdtsperf_component_ptr_t component;
  rwdtsperf_instance_ptr_t instance;

  // Validate input parameters
  component = (rwdtsperf_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);
  instance = (rwdtsperf_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwdtsperf_instance_ptr_t);

  rwdtsperf_instance_free (component, instance);
}

static void
rwdtsperf__component__instance_start(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					RwTaskletPluginInstanceHandle *h_instance)
{
  rwdtsperf_component_ptr_t component;
  rwdtsperf_instance_ptr_t instance;

  // Validate input parameters
  component = (rwdtsperf_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);
  instance = (rwdtsperf_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwdtsperf_instance_ptr_t);

  // Call the tasklet instance start routine
  rwdtsperf_instance_start(component, instance);
  
  // The instance is started so print our RW.DTSPerf message
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	       RWTRACE_CATEGORY_RWTASKLET,
	       "\n================================================================================\n\n"
	       "RW.Dtsperf -- Tasklet is started!\n\n"
	       "================================================================================\n");
}

static void
rwdtsperf__component__instance_stop(
RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					RwTaskletPluginInstanceHandle *h_instance)
{
  rwdtsperf_component_ptr_t component;
  rwdtsperf_instance_ptr_t instance;

  // Validate input parameters
  component = (rwdtsperf_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);
  instance = (rwdtsperf_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwdtsperf_instance_ptr_t);

  rwdtsperf_instance_stop (component, instance);

  RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance,
      RWTRACE_CATEGORY_RWTASKLET,
           "rwdtsperf-c transactions stopped, but tasklet is still running.");
}

static void
rwdtsperf_c_extension_init(RwdtsperfCExtension *plugin)
{
}

static void
rwdtsperf_c_extension_class_init(RwdtsperfCExtensionClass *klass)
{
}

static void
rwdtsperf_c_extension_class_finalize(RwdtsperfCExtensionClass *klass)
{
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
