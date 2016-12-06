
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
#include "rwtasklet.h"
#include "rwmsgbroker-c.h"
#include <rwvcs.h>
#include "rwmemlogdts.h"
                                                                                           
RwTaskletPluginComponentHandle *
rwmsgbroker__component__component_init(RwTaskletPluginComponent *self)
{
  rwmsgbroker_component_ptr_t component;
  RwTaskletPluginComponentHandle *h_component;
  gpointer handle;

  // Register the RW.Init types
  RW_CF_TYPE_REGISTER(rwmsgbroker_component_ptr_t);
  RW_CF_TYPE_REGISTER(rwmsgbroker_instance_ptr_t);

  // Allocate a new rwmsgbroker_component structure
  component = RW_CF_TYPE_MALLOC0(sizeof(*component), rwmsgbroker_component_ptr_t);
  RW_CF_TYPE_VALIDATE(component, rwmsgbroker_component_ptr_t);

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_COMPONENT_HANDLE, 0);
  h_component = RW_TASKLET_PLUGIN_COMPONENT_HANDLE(handle);
  h_component->priv = (gpointer) component;

  // Return the handle to the rwtasklet component
  return h_component;
}

void
rwmsgbroker__component__component_deinit(RwTaskletPluginComponent *self,
					  RwTaskletPluginComponentHandle *h_component)
{
}

static RwTaskletPluginInstanceHandle *
rwmsgbroker__component__instance_alloc(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					struct rwtasklet_info_s * rwtasklet_info,
					RwTaskletPlugin_RWExecURL* instance_url)
{
  rwmsgbroker_component_ptr_t component;
  rwmsgbroker_instance_ptr_t instance;
  RwTaskletPluginInstanceHandle *h_instance;
  gpointer handle;

  // Validate input parameters
  component = (rwmsgbroker_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwmsgbroker_component_ptr_t);

  // Allocate a new rwmsgbroker_instance structure
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwmsgbroker_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwmsgbroker_instance_ptr_t);

  instance->rwtasklet_info = rwtasklet_info;

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_INSTANCE_HANDLE, 0);
  h_instance = RW_TASKLET_PLUGIN_INSTANCE_HANDLE(handle);
  h_instance->priv = (gpointer) instance;

  // Return the handle to the rwtasklet instance
  return h_instance;
}

void
rwmsgbroker__component__instance_free(RwTaskletPluginComponent *self,
				       RwTaskletPluginComponentHandle *h_component,
				       RwTaskletPluginInstanceHandle *h_instance)
{
}

rw_status_t
rwmsg_broker_dts_registration (rwmsgbroker_instance_ptr_t instance);

static void
rwmsgbroker__component__instance_start(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					RwTaskletPluginInstanceHandle *h_instance)
{
  rwmsgbroker_component_ptr_t component;
  rwmsgbroker_instance_ptr_t instance;
  rwvcs_zk_module_ptr_t rwvcs_zk_module;

  // Validate input parameters
  component = (rwmsgbroker_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwmsgbroker_component_ptr_t);
  instance = (rwmsgbroker_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwmsgbroker_instance_ptr_t);

  // The instance is started so print a debug message
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	             RWTRACE_CATEGORY_RWTASKLET,
	             "RW.MsgBroker -- Tasklet [%d] is started on VM [%d]!",
               instance->rwtasklet_info->identity.rwtasklet_instance_id, //0);
               instance->rwtasklet_info->rwvcs->identity.rwvm_instance_id);

  rwvcs_instance_ptr_t rwvcs = instance->rwtasklet_info->rwvcs;
  RW_ASSERT(rwvcs);
  int rwvm_instance_id = rwvcs->identity.rwvm_instance_id;
  RW_ASSERT(rwvcs->pb_rwmanifest
            && rwvcs->pb_rwmanifest->init_phase
            && rwvcs->pb_rwmanifest->init_phase->settings
            && rwvcs->pb_rwmanifest->init_phase->settings->rwmsg);
  int multi_broker = (rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker &&
                      rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker->has_enable &&
                      rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker->enable);

  char *ext_ip_address = instance->rwtasklet_info->rwvcs->identity.vm_ip_address;
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "RW.MsgBroker -- Tasklet [%d] is started on VM [%d] ip-addr [%s] multi_broker [%d]!\n",
               instance->rwtasklet_info->identity.rwtasklet_instance_id,
               rwvm_instance_id, ext_ip_address,
               multi_broker);

  int sid = 1;
  rwvcs_zk_module = RWVX_GET_ZK_MODULE((rwvx_instance_t*)(instance->rwtasklet_info->rwvx));
  instance->broker = rwmsg_broker_create(sid,
                                         (multi_broker?rwvm_instance_id:0),
                                         ext_ip_address,
                                         instance->rwtasklet_info->rwsched_instance,
                                         instance->rwtasklet_info->rwsched_tasklet_info,
                                         rwvcs_zk_module,
                                         rwtasklet_info_is_collapse_thread(instance->rwtasklet_info), /* mainq */
                                         instance->rwtasklet_info->rwmsg_endpoint,
                                         instance->rwtasklet_info);

  RW_ASSERT(instance->broker);

  rw_status_t rs;
  rs = rwmsg_broker_dts_registration (instance);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  instance->broker->rwmemlog = rwmemlog_instance_alloc("MSGBroker", 0, 0);
  rwmemlog_instance_dts_register(instance->broker->rwmemlog, instance->rwtasklet_info, instance->dts_h);
}

  static void
rwmsgbroker__component__instance_stop(
    RwTaskletPluginComponent *self,
		RwTaskletPluginComponentHandle *h_component,
		RwTaskletPluginInstanceHandle *h_instance)
{
  rwmsgbroker_component_ptr_t component;
  rwmsgbroker_instance_ptr_t instance;

  // Validate input parameters
  component = (rwmsgbroker_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwmsgbroker_component_ptr_t);
  instance = (rwmsgbroker_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwmsgbroker_instance_ptr_t);

  // The instance is started so print a debug message
  RWTRACE_WARN(instance->rwtasklet_info->rwtrace_instance,
	             RWTRACE_CATEGORY_RWTASKLET,
	             "RW.MsgBroker[%d] -- stop method undefined",
               instance->rwtasklet_info->identity.rwtasklet_instance_id);
}

static void
rwmsgbroker_c_extension_init(RwmsgbrokerCExtension *plugin)
{
}

static void
rwmsgbroker_c_extension_class_init(RwmsgbrokerCExtensionClass *klass)
{
}

static void
rwmsgbroker_c_extension_class_finalize(RwmsgbrokerCExtensionClass *klass)
{
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
