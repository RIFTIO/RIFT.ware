
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
#include "rwlogd-c.h"
#include "rwlogd_common.h"
#include "rwlogd.pb-c.h"
#include "rwvcs.h"

RW_CF_TYPE_DEFINE("RW.Init RWTasklet Component Type", rwlogd_component_ptr_t);
RW_CF_TYPE_DEFINE("RW.Init RWTasklet Instance Type", rwlogd_instance_ptr_t);


RwTaskletPluginComponentHandle *
rwlogd__component__component_init(RwTaskletPluginComponent *self)
{
  rwlogd_component_ptr_t component;
  RwTaskletPluginComponentHandle *h_component;
  gpointer handle;

  // Register the RW.Init types
  RW_CF_TYPE_REGISTER(rwlogd_component_ptr_t);
  RW_CF_TYPE_REGISTER(rwlogd_instance_ptr_t);

  // Allocate a new rwlogd_component structure
  component = RW_CF_TYPE_MALLOC0(sizeof(*component), rwlogd_component_ptr_t);
  RW_CF_TYPE_VALIDATE(component, rwlogd_component_ptr_t);

  // Allocate a gobject for the handle
  handle = g_object_new(RW_TASKLET_PLUGIN_TYPE_COMPONENT_HANDLE, 0);
  h_component = RW_TASKLET_PLUGIN_COMPONENT_HANDLE(handle);
  h_component->priv = (gpointer) component;

  // Return the handle to the rwtasklet component
  return h_component;
}

void
rwlogd__component__component_deinit(RwTaskletPluginComponent *self,
					  RwTaskletPluginComponentHandle *h_component)
{
}

static RwTaskletPluginInstanceHandle *
rwlogd__component__instance_alloc(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					struct rwtasklet_info_s * rwtasklet_info,
					RwTaskletPlugin_RWExecURL* instance_url)
{
  rwlogd_component_ptr_t component;
  rwlogd_instance_ptr_t instance;
  RwTaskletPluginInstanceHandle *h_instance;
  gpointer handle;

  // Validate input parameters
  component = (rwlogd_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwlogd_component_ptr_t);

  // Allocate a new rwlogd_instance structure
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwlogd_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwlogd_instance_ptr_t);

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
rwlogd__component__instance_free(RwTaskletPluginComponent *self,
				       RwTaskletPluginComponentHandle *h_component,
				       RwTaskletPluginInstanceHandle *h_instance)
{
}


static rw_status_t 
rwlogd_create_client_endpoint(rwlogd_instance_ptr_t instance)
{
  // Create endpoint
  rwmsg_clichan_t *log_cli_chan;

  RWLOGD__INITCLIENT(&instance->rwlogd_cli);
  log_cli_chan = rwmsg_clichan_create(instance->rwtasklet_info->rwmsg_endpoint);
  rwmsg_clichan_add_service(log_cli_chan, &instance->rwlogd_cli.base);
  RW_ASSERT(instance->rwlogd_cli.base.rw_context);
  if (!instance->rwlogd_cli.base.rw_context) {
    return RW_STATUS_FAILURE;
  }

  RWLOGD_PEER_API__INITCLIENT(&instance->rwlogd_peer_msg_client);
  rwmsg_clichan_add_service(log_cli_chan, 
                            &instance->rwlogd_peer_msg_client.base);
  RW_ASSERT(instance->rwlogd_peer_msg_client.base.rw_context); 
  if (!instance->rwlogd_peer_msg_client.base.rw_context) {
    return RW_STATUS_FAILURE;
  }

  instance->cc = log_cli_chan;
  return RW_STATUS_SUCCESS;
}


void rwlogd_start_tasklet(rwlogd_instance_ptr_t instance, bool dts_register,char *rwlog_filename,char *filter_shm_name,
                          const char *schema_name)
{
  rwlogd_tasklet_instance_ptr_t rwlogd_instance_data;
  rwlogd_instance_data = (rwlogd_tasklet_instance_ptr_t) RW_MALLOC0(sizeof(struct rwlogd_tasklet_instance_s));
  rwlogd_instance_data->file_status.fd = -1;
  rwlogd_instance_data->log_buffer_size = CIRCULAR_BUFFER_SIZE;
  rwlogd_instance_data->rwlogd_instance = (void *)instance;

  rwlogd_instance_data->bootstrap_time = BOOTSTRAP_TIME;
  rwlogd_instance_data->bootstrap_severity = RW_LOG_LOG_SEVERITY_ERROR;
  rwlogd_instance_data->default_console_severity = RW_LOG_LOG_SEVERITY_ERROR;

  struct rwvcs_instance_s* vcs_inst = (struct rwvcs_instance_s*)(instance->rwtasklet_info->rwvcs);

  if(vcs_inst && vcs_inst->pb_rwmanifest &&
     vcs_inst->pb_rwmanifest->bootstrap_phase && 
     vcs_inst->pb_rwmanifest->bootstrap_phase->log &&
     vcs_inst->pb_rwmanifest->bootstrap_phase->log->has_enable) {

    if(vcs_inst->pb_rwmanifest->bootstrap_phase->log->has_bootstrap_time) {
      rwlogd_instance_data->bootstrap_time = vcs_inst->pb_rwmanifest->bootstrap_phase->log->bootstrap_time*RWLOGD_TICKS_PER_SEC;
    }
    if(vcs_inst->pb_rwmanifest->bootstrap_phase->log->has_severity &&
       vcs_inst->pb_rwmanifest->bootstrap_phase->log->severity < RW_LOG_LOG_SEVERITY_MAX_VALUE) {
      rwlogd_instance_data->bootstrap_severity = vcs_inst->pb_rwmanifest->bootstrap_phase->log->severity;
    }
    if(vcs_inst->pb_rwmanifest->bootstrap_phase->log->has_console_severity &&
       vcs_inst->pb_rwmanifest->bootstrap_phase->log->console_severity < RW_LOG_LOG_SEVERITY_MAX_VALUE) {
      rwlogd_instance_data->default_console_severity = vcs_inst->pb_rwmanifest->bootstrap_phase->log->console_severity;
    }
  }


  rwlogd_instance_data->log_buffer = RW_MALLOC0(rwlogd_instance_data->log_buffer_size);
  rwlogd_instance_data->curr_offset = 0;
  rwlogd_instance_data->curr_used_offset = rwlogd_instance_data->log_buffer_size;  /* Point this to end of buffer to indicate full buffer is free */

  instance->rwlogd_info = rwlogd_instance_data;
 
  instance->rwlogd_info->rwlogd_list_ring =  rwlogd_create_consistent_hash_ring(RWLOGD_DEFAULT_NODE_REPLICA); 
  /* Create List to maintains peer RwLogd list */
  RW_SKLIST_PARAMS_DECL(rwlogd_list_, rwlogd_peer_node_entry_t,rwtasklet_instance_id, rw_sklist_comp_int, element);
  RW_SKLIST_INIT(&(instance->rwlogd_info->peer_rwlogd_list),&rwlogd_list_);

  instance->dynschema_app_state = RW_MGMT_SCHEMA_APPLICATION_STATE_INITIALIZING;

  if (dts_register)
  {
    rwlog_dts_registration(instance);  
  }
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
	       RWTRACE_CATEGORY_RWTASKLET,
	       "\n================================================================================\n\n"
	       "RW.Logd -- Tasklet is started (%d) !\n\n"
	       "================================================================================\n",
         instance->rwtasklet_info->identity.rwtasklet_instance_id);

  rwlogd_create_client_endpoint(instance);
  rwlogd_create_server_endpoint(instance);
  rwlogd_start(instance,rwlog_filename,filter_shm_name,schema_name);

  if(schema_name) {
    instance->dynschema_app_state = RW_MGMT_SCHEMA_APPLICATION_STATE_READY;
  }

}

static void
rwlogd__component__instance_start(RwTaskletPluginComponent *self,
					RwTaskletPluginComponentHandle *h_component,
					RwTaskletPluginInstanceHandle *h_instance)
{
  rwlogd_component_ptr_t component;
  rwlogd_instance_ptr_t instance;

  // Validate input parameters
  component = (rwlogd_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwlogd_component_ptr_t);
  instance = (rwlogd_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwlogd_instance_ptr_t);

  rwlogd_start_tasklet(instance, true,NULL,NULL,NULL);
}

static void
rwlogd__component__instance_stop(
    RwTaskletPluginComponent * self,
    RwTaskletPluginComponentHandle * h_component,
    RwTaskletPluginInstanceHandle * h_instance)
{
  rwlogd_component_ptr_t component;
  rwlogd_instance_ptr_t instance;

  // Validate input parameters
  component = (rwlogd_component_ptr_t) h_component->priv;
  RW_CF_TYPE_VALIDATE(component, rwlogd_component_ptr_t);
  instance = (rwlogd_instance_ptr_t) h_instance->priv;
  RW_CF_TYPE_VALIDATE(instance, rwlogd_instance_ptr_t);
  rwmsg_clichan_halt(instance->cc);
  rwmsg_srvchan_halt(instance->sc);
  rwdts_api_deinit(instance->dts_h);

  RWTRACE_WARN(instance->rwtasklet_info->rwtrace_instance,
      RWTRACE_CATEGORY_RWTASKLET,
      "RW.Logd[%d] -- Stopping ",
      instance->rwtasklet_info->identity.rwtasklet_instance_id);
}

static void
rwlogd_c_extension_init(RwlogdCExtension *plugin)
{
}

static void
rwlogd_c_extension_class_init(RwlogdCExtensionClass *klass)
{
}

static void
rwlogd_c_extension_class_finalize(RwlogdCExtensionClass *klass)
{
}


#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN
