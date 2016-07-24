
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include "rwcli-c.h"

RW_CF_TYPE_DEFINE("RW.Cli RWTasklet Component Type", rwcli_component_ptr_t);

RW_CF_TYPE_DEFINE("RW.Cli RWTasklet Instance Type", rwcli_instance_ptr_t);

RwTaskletPluginComponentHandle *
rwcli__component__component_init(RwTaskletPluginComponent *self)
{
  rwcli_component_ptr_t component;
  RwTaskletPluginComponentHandle *h_component;
  gpointer handle;

  // Register the RW.Init types
  RW_CF_TYPE_REGISTER(rwcli_component_ptr_t);
  RW_CF_TYPE_REGISTER(rwcli_instance_ptr_t);

  // Allocate a new rwcli_component structure
  component = rwcli_component_init();
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_COMPONENT_HANDLE, 0);
  h_component = RW_TASKLET_PLUGIN_COMPONENT_HANDLE(handle);
  h_component->priv = (gpointer) component;

  // Return the handle to the rwtasklet component
  return h_component;
}

void
rwcli__component__component_deinit(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component)
{
}

static RwTaskletPluginInstanceHandle *
rwcli__component__instance_alloc(RwTaskletPluginComponent *self,
				 RwTaskletPluginComponentHandle *h_component,
				 struct rwtasklet_info_s * rwtasklet_info,
				 RwTaskletPlugin_RWExecURL* instance_url)
{
  rwcli_component_ptr_t component;
  rwcli_instance_ptr_t instance;
  RwTaskletPluginInstanceHandle *h_instance;
  gpointer handle;

  // Validate input parameters
  component = (rwcli_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);

  // Allocate a new rwcli_instance structure
  instance = rwcli_instance_alloc(component, rwtasklet_info, instance_url);
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_INSTANCE_HANDLE, 0);
  h_instance = RW_TASKLET_PLUGIN_INSTANCE_HANDLE(handle);
  h_instance->priv = (gpointer) instance;

  // Return the handle to the rwtasklet instance
  return h_instance;
}

void
rwcli__component__instance_free(RwTaskletPluginComponent *self,
				     RwTaskletPluginComponentHandle *h_component,
				     RwTaskletPluginInstanceHandle *h_instance)
{
}

static void
rwcli__component__instance_start(RwTaskletPluginComponent *self,
				      RwTaskletPluginComponentHandle *h_component,
				      RwTaskletPluginInstanceHandle *h_instance)
{
  rwcli_component_ptr_t component;
  rwcli_instance_ptr_t instance;

  // Validate input parameters
  component = (rwcli_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);
  instance = (rwcli_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);

  // Call the tasklet instance start routine
  rwcli_instance_start(component, instance);
  
  // The instance is started so print our RW.Cli message
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
      	       RWTRACE_CATEGORY_RWTASKLET,
      	       "RW.Cli[%d] -- Tasklet is started!",
               instance->rwtasklet_info->identity.rwtasklet_instance_id);
}

static void
rwcli__component__instance_stop(RwTaskletPluginComponent *self,
				      RwTaskletPluginComponentHandle *h_component,
				      RwTaskletPluginInstanceHandle *h_instance)
{
  rwcli_component_ptr_t component;
  rwcli_instance_ptr_t instance;

  // Validate input parameters
  component = (rwcli_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwcli_component_ptr_t);
  instance = (rwcli_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwcli_instance_ptr_t);

  // The instance is started so print our RW.Cli message
  RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance,
      	       RWTRACE_CATEGORY_RWTASKLET,
      	       "rwcli.instance_stop() is still a stub function");
}


static void
rwcli_c_extension_init(RwcliCExtension *plugin)
{
}

static void
rwcli_c_extension_class_init(RwcliCExtensionClass *klass)
{
}

static void
rwcli_c_extension_class_finalize(RwcliCExtensionClass *klass)
{
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
