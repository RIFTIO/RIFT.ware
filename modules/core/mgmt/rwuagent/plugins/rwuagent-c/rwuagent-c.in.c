/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwuagent-c.in.c
 * @author Tom Seidenberg
 * @date 2014/04/03
 * @brief RW.uAgent tasklet plugin vala input file
 */

#include "rwuagent-c.h"
#include "rwtrace.h"

RW_CF_TYPE_DECLARE(rwuagent_component_t);
RW_CF_TYPE_DECLARE(rwuagent_instance_t);

RwTaskletPluginComponentHandle *
rwuagent__component__component_init(RwTaskletPluginComponent *self)
{
  // Register the RW.Init types
  RW_CF_TYPE_REGISTER(rwuagent_component_t);
  RW_CF_TYPE_REGISTER(rwuagent_instance_t);

  // Allocate a new rwuagent_component structure
  rwuagent_component_t component = rwuagent_component_init();
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);

  // Allocate a gobject for the handle
  gpointer handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_COMPONENT_HANDLE, 0);
  RwTaskletPluginComponentHandle* h_component = RW_TASKLET_PLUGIN_COMPONENT_HANDLE(handle);
  h_component->priv = (gpointer) component;

  // Return the handle to the rwtasklet component
  return h_component;
}

void
rwuagent__component__component_deinit(RwTaskletPluginComponent *self,
                                      RwTaskletPluginComponentHandle *h_component)
{
}

static RwTaskletPluginInstanceHandle *
rwuagent__component__instance_alloc(RwTaskletPluginComponent *self,
                                    RwTaskletPluginComponentHandle *h_component,
                                    struct rwtasklet_info_s * rwtasklet_info,
                                    RwTaskletPlugin_RWExecURL* instance_url)
{
  // Validate input parameters
  rwuagent_component_t component = (rwuagent_component_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);

  // Allocate a new rwuagent_instance structure
  rwuagent_instance_t instance = rwuagent_instance_alloc(component, rwtasklet_info, instance_url);
  RW_CF_TYPE_VALIDATE(instance, rwuagent_instance_t);

  // Allocate a gobject for the handle
  gpointer handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_INSTANCE_HANDLE, 0);
  RwTaskletPluginInstanceHandle* h_instance = RW_TASKLET_PLUGIN_INSTANCE_HANDLE(handle);
  h_instance->priv = (gpointer) instance;

  // Return the handle to the rwtasklet instance
  return h_instance;
}

void
rwuagent__component__instance_free(RwTaskletPluginComponent *self,
                                   RwTaskletPluginComponentHandle *h_component,
                                   RwTaskletPluginInstanceHandle *h_instance)
{
}

static void
rwuagent__component__instance_start(RwTaskletPluginComponent *self,
                                    RwTaskletPluginComponentHandle *h_component,
                                    RwTaskletPluginInstanceHandle *h_instance)
{
  // Validate input parameters
  rwuagent_component_t component = (rwuagent_component_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);
  rwuagent_instance_t instance = (rwuagent_instance_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwuagent_instance_t);

  // Call the tasklet instance start routine
  rwuagent_instance_start(component, instance);
  
  // The instance is started so print our RW.uAgent message
  RWTRACE_INFO(rwuagent_get_rwtrace_instance(instance),
               RWTRACE_CATEGORY_RWTASKLET,
               "RW.uAgent -- Tasklet plugin is started!");
}

  static void
rwuagent__component__instance_stop(RwTaskletPluginComponent *self,
                                    RwTaskletPluginComponentHandle *h_component,
                                    RwTaskletPluginInstanceHandle *h_instance)
{
  // Validate input parameters
  rwuagent_component_t component = (rwuagent_component_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);
  rwuagent_instance_t instance = (rwuagent_instance_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwuagent_instance_t);

  // The instance is started so print our RW.uAgent message
  RWTRACE_ERROR(rwuagent_get_rwtrace_instance(instance),
               RWTRACE_CATEGORY_RWTASKLET,
               "rwuagent.instance_stop() is still a stub function");
}


static void
rwuagent_c_extension_init(RwuagentCExtension *plugin)
{
}

static void
rwuagent_c_extension_class_init(RwuagentCExtensionClass *klass)
{
}

static void
rwuagent_c_extension_class_finalize(RwuagentCExtensionClass *klass)
{
}

// The space below gets filled in by vala automatically
#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
