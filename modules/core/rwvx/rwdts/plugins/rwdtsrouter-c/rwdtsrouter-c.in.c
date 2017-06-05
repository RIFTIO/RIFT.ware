
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include "rwdtsrouter-c.h"
#include "rwvcs.h"

RW_CF_TYPE_DEFINE("RW.DtsRouter RWTasklet Component Type", rwdtsrouter_component_ptr_t);
RW_CF_TYPE_DEFINE("RW.DtsRouter RWTasklet Instance Type",  rwdtsrouter_instance_ptr_t);

RwTaskletPluginComponentHandle *
rwdtsrouter__component__component_init(RwTaskletPluginComponent *self)
{
  rwdtsrouter_component_ptr_t component;
  
  // Register the RW.Init types
  RW_CF_TYPE_REGISTER(rwdtsrouter_component_ptr_t);
  RW_CF_TYPE_REGISTER(rwdtsrouter_instance_ptr_t);

  // Allocate a new rwdtsrouter_component structure
  component = RW_CF_TYPE_MALLOC0(sizeof(*component), rwdtsrouter_component_ptr_t);
  RW_CF_TYPE_VALIDATE(component, rwdtsrouter_component_ptr_t);

  // Allocate a gobject for the handle
  gpointer handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_COMPONENT_HANDLE, 0);
  RwTaskletPluginComponentHandle* h_component = RW_TASKLET_PLUGIN_COMPONENT_HANDLE(handle);
  h_component->priv = (gpointer) component;

  // Return the handle to the rwtasklet component
  return h_component;
}

void
rwdtsrouter__component__component_deinit(RwTaskletPluginComponent *self,
                                         RwTaskletPluginComponentHandle *h_component)
{
}

static RwTaskletPluginInstanceHandle *
rwdtsrouter__component__instance_alloc(RwTaskletPluginComponent *self,
                                       RwTaskletPluginComponentHandle *h_component,
                                       struct rwtasklet_info_s * rwtasklet_info,
                                       RwTaskletPlugin_RWExecURL* instance_url)
{
  // Validate input parameters
  rwdtsrouter_component_ptr_t component = (rwdtsrouter_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwdtsrouter_component_ptr_t);

  // Allocate a new rwdtsrouter_instance structure
  rwdtsrouter_instance_ptr_t instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwdtsrouter_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwdtsrouter_instance_ptr_t);

  instance->rwtasklet_info = rwtasklet_info;

  // Allocate a gobject for the handle
  gpointer handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_INSTANCE_HANDLE, 0);
  RwTaskletPluginInstanceHandle* h_instance = RW_TASKLET_PLUGIN_INSTANCE_HANDLE(handle);
  h_instance->priv = (gpointer) instance;

  // Return the handle to the rwtasklet instance
  return h_instance;
}

void
rwdtsrouter__component__instance_free(RwTaskletPluginComponent *self,
                                      RwTaskletPluginComponentHandle *h_component,
                                      RwTaskletPluginInstanceHandle *h_instance)
{
}

static void
rwdtsrouter__component__instance_start(RwTaskletPluginComponent *self,
                                       RwTaskletPluginComponentHandle *h_component,
                                       RwTaskletPluginInstanceHandle *h_instance)
{
  // Validate input parameters
  rwdtsrouter_component_ptr_t component = (rwdtsrouter_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwdtsrouter_component_ptr_t);

  rwdtsrouter_instance_ptr_t instance = (rwdtsrouter_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwdtsrouter_instance_ptr_t);
  rwvcs_instance_ptr_t rwvcs = instance->rwtasklet_info->rwvcs;

  RW_ASSERT(rwvcs->pb_rwmanifest                                               
            && rwvcs->pb_rwmanifest->init_phase                                
            && rwvcs->pb_rwmanifest->init_phase->settings                      
            && rwvcs->pb_rwmanifest->init_phase->settings->rwdtsrouter);
  int multi_dtsrouter = (rwvcs->pb_rwmanifest->init_phase->settings->rwdtsrouter->multi_dtsrouter &&
                      rwvcs->pb_rwmanifest->init_phase->settings->rwdtsrouter->multi_dtsrouter->has_enable &&
                      rwvcs->pb_rwmanifest->init_phase->settings->rwdtsrouter->multi_dtsrouter->enable);      

  int vm_id = (multi_dtsrouter)? rwvcs->identity.rwvm_instance_id: 0;

  instance->router = rwdtsrouter_instance_start(instance->rwtasklet_info, vm_id,
                                                RWVX_GET_ZK_MODULE((rwvx_instance_t*)(instance->rwtasklet_info->rwvx)));

  // The instance is started so print a debug message
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
               "RW.DtsRouter -- Tasklet [%d] is started!",
               instance->rwtasklet_info->identity.rwtasklet_instance_id);
}

static void
rwdtsrouter__component__instance_stop(
                RwTaskletPluginComponent *self,
                RwTaskletPluginComponentHandle *h_component,
                RwTaskletPluginInstanceHandle *h_instance)
{

  // Validate input parameters
  rwdtsrouter_component_ptr_t component = (rwdtsrouter_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwdtsrouter_component_ptr_t);

  rwdtsrouter_instance_ptr_t instance = (rwdtsrouter_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwdtsrouter_instance_ptr_t);

  // The instance is started so print a debug message
  RWTRACE_WARN(instance->rwtasklet_info->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "RW.DtsRouter[%d] -- stop method undefined",
                     instance->rwtasklet_info->identity.rwtasklet_instance_id);
}

static void
rwdtsrouter_c_extension_init(RwdtsrouterCExtension *plugin)
{
}

static void
rwdtsrouter_c_extension_class_init(RwdtsrouterCExtensionClass *klass)
{
}

static void
rwdtsrouter_c_extension_class_finalize(RwdtsrouterCExtensionClass *klass)
{
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
