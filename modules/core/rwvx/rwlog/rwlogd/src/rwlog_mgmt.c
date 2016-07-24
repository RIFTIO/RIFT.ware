
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwlog_mgmt.c
 * @date 11/09/2014
 * @brief RW.Log Tasklet Management Implementation
 *
 */
#include "rwlogd_common.h"
#include "rwdts.h"
#include <pcap/pcap.h>
#include "rwlogd_plugin_mgr.h"
#include "rwlogd_display.h"
#include "rwvcs_rwzk.h"
#include "rwshell_mgmt.h"
#include "rwdynschema.h"

#define MULTI_VM_LOG_DISPLAY

/*
 * this is for CLI pretty print display conversion from enum to string 
 */
char *seve[]={"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug"};


static rwdts_member_rsp_code_t
rwlog_mgmt_handle_log_query_dts_internal (const rwdts_xact_info_t* xact_info,
                                 RWDtsQueryAction         action,
                                 const rw_keyspec_path_t*      key,
                                 const ProtobufCMessage*  msg,
                                 uint32_t credits,
                                 void *getnext_ptr);

//LCOV_EXCL_START
static rw_status_t 
rwlog_apply_category_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name, 
                   RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *category);

void
rwlog_generate_bpf_filter(char *filter_str)
{
  struct bpf_program  pcap_program;
  pcap_t * pc;
  pc = pcap_open_dead(DLT_EN10MB, RWLOG_MGMT_PCAP_SNAPLEN);
  if (pc) {
    if (pcap_compile(pc, &pcap_program, filter_str, 1, PCAP_NETMASK_UNKNOWN) != -1) {
      log_bpf_filter_t bpf_filter;
      uint32_t i;
      bpf_filter.bf_len = pcap_program.bf_len;
      for (i=0; i< bpf_filter.bf_len; i++) {
        bpf_filter.bpf_inst[i].code = pcap_program.bf_insns[i].code;
        bpf_filter.bpf_inst[i].jt = pcap_program.bf_insns[i].jt;
        bpf_filter.bpf_inst[i].jf = pcap_program.bf_insns[i].jf;
        bpf_filter.bpf_inst[i].k = pcap_program.bf_insns[i].k;
      }
    }
  }
  pcap_close(pc);

  /* Store the strign and bpf filter in shard memory here */

  return;
}
#if 0
static void 
rwlog_show_log_apply_packet_filter(rwlogd_instance_ptr_t instance, RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category_Packet) *packet)
{
  char apply_filter[RWLOG_MGMT_FILTER_SZ] = "\0";
  char *filter_ptr = &apply_filter[0];
  bool use_and = FALSE;
  bool proto_set = FALSE;

  if (packet->protocol) {
    sprintf(filter_ptr, "%s%c", packet->protocol, ' ');
    filter_ptr+= (strlen(packet->protocol) + 1);
    proto_set = TRUE;
  }
  if (packet->src_net) {
    if (proto_set == TRUE) {
      sprintf(filter_ptr, "%s%s%c", "and src net ", packet->src_net, ' ');
      filter_ptr+= (strlen(packet->src_net) + 13);
      use_and = TRUE;
    } else {
      sprintf(filter_ptr, "%s%s%c", "src net ", packet->src_net, ' ');
      filter_ptr+= (strlen(packet->src_net) + 9);
      use_and = TRUE;
    }
  }
  if (packet->dst_net) {
    if ((use_and == TRUE)||(proto_set == TRUE)) {
      sprintf(filter_ptr, "%s%s%c", "and dst net ", packet->dst_net, ' ');
      filter_ptr+= (strlen(packet->dst_net) + 13);
      use_and = TRUE;
    } else {
      sprintf(filter_ptr, "%s%s%c", "dst net ", packet->dst_net, ' ');
      filter_ptr+= (strlen(packet->dst_net) + 9);
      use_and = TRUE;
    }
  }
  if (packet->has_src_port) {
    if (use_and == TRUE) {
      sprintf(filter_ptr, "%s%d%c", "and src port ", packet->src_port, ' ');
      filter_ptr+=18;
    } else {
      sprintf(filter_ptr, "%s%d%c", "src port ", packet->src_port, ' ');
      filter_ptr+=14;
      use_and = TRUE;
    }
  }
  if (packet->has_dst_port) {
    if (use_and == TRUE) {
      sprintf(filter_ptr, "%s%d", "and dst port ", packet->dst_port);
      filter_ptr+= 17;
    } else {
      sprintf(filter_ptr, "%s%d", "dst port ", packet->dst_port);
      filter_ptr+= 13;
    }
  }

  rwlog_generate_bpf_filter(apply_filter);

  return;
}
#endif


static void 
rwlog_rpc_apply_packet_filter(rwlogd_instance_ptr_t instance, RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter_Category_Packet) *packet)
{
  char apply_filter[RWLOG_MGMT_FILTER_SZ] = "\0";
  char *filter_ptr = &apply_filter[0];
  bool use_and = FALSE;
  bool proto_set = FALSE;

  if (packet->protocol) {
    sprintf(filter_ptr, "%s%c", packet->protocol, ' ');
    filter_ptr+= (strlen(packet->protocol) + 1);
    proto_set = TRUE;
  }
  if (packet->src_net) {
    if (proto_set == TRUE) {
      sprintf(filter_ptr, "%s%s%c", "and src net ", packet->src_net, ' ');
      filter_ptr+= (strlen(packet->src_net) + 13);
      use_and = TRUE;
    } else {
      sprintf(filter_ptr, "%s%s%c", "src net ", packet->src_net, ' ');
      filter_ptr+= (strlen(packet->src_net) + 9);
      use_and = TRUE;
    }
  }
  if (packet->dst_net) {
    if ((use_and == TRUE)||(proto_set == TRUE)) {
      sprintf(filter_ptr, "%s%s%c", "and dst net ", packet->dst_net, ' ');
      filter_ptr+= (strlen(packet->dst_net) + 13);
      use_and = TRUE;
    } else {
      sprintf(filter_ptr, "%s%s%c", "dst net ", packet->dst_net, ' ');
      filter_ptr+= (strlen(packet->dst_net) + 9);
      use_and = TRUE;
    }
  }
  if (packet->has_src_port) {
    if (use_and == TRUE) {
      sprintf(filter_ptr, "%s%d%c", "and src port ", packet->src_port, ' ');
      filter_ptr+=18;
    } else {
      sprintf(filter_ptr, "%s%d%c", "src port ", packet->src_port, ' ');
      filter_ptr+=14;
      use_and = TRUE;
    }
  }
  if (packet->has_dst_port) {
    if (use_and == TRUE) {
      sprintf(filter_ptr, "%s%d", "and dst port ", packet->dst_port);
      filter_ptr+= 17;
    } else {
      sprintf(filter_ptr, "%s%d", "dst port ", packet->dst_port);
      filter_ptr+= 13;
    }
  }

  rwlog_generate_bpf_filter(apply_filter);

  return;
}

static void 
rwlog_apply_packet_filter(rwlogd_instance_ptr_t instance, RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category_Packet) * packet)
{
  char apply_filter[RWLOG_MGMT_FILTER_SZ] = "\0";
  char *filter_ptr = &apply_filter[0];
  bool use_and = FALSE;
  bool proto_set = FALSE;

  if (packet->protocol) {
    sprintf(filter_ptr, "%s%c", packet->protocol, ' ');
    filter_ptr+= (strlen(packet->protocol) + 1);
    proto_set = TRUE;
  }
  if (packet->src_net) {
    if (proto_set == TRUE) {
      sprintf(filter_ptr, "%s%s%c", "and src net ", packet->src_net, ' ');
      filter_ptr+= (strlen(packet->src_net) + 13);
      use_and = TRUE;
    } else {
      sprintf(filter_ptr, "%s%s%c", "src net ", packet->src_net, ' ');
      filter_ptr+= (strlen(packet->src_net) + 9);
      use_and = TRUE;
    }
  }
  if (packet->dst_net) {
    if ((use_and == TRUE)||(proto_set == TRUE)) {
      sprintf(filter_ptr, "%s%s%c", "and dst net ", packet->dst_net, ' ');
      filter_ptr+= (strlen(packet->dst_net) + 13);
      use_and = TRUE;
    } else {
      sprintf(filter_ptr, "%s%s%c", "dst net ", packet->dst_net, ' ');
      filter_ptr+= (strlen(packet->dst_net) + 9);
      use_and = TRUE;
    }
  }
  if (packet->has_src_port) {
    if (use_and == TRUE) {
      sprintf(filter_ptr, "%s%d%c", "and src port ", packet->src_port, ' ');
      filter_ptr+=18;
    } else {
      sprintf(filter_ptr, "%s%d%c", "src port ", packet->src_port, ' ');
      filter_ptr+=14;
      use_and = TRUE;
    }
  }
  if (packet->has_dst_port) {
    if (use_and == TRUE) {
      sprintf(filter_ptr, "%s%d", "and dst port ", packet->dst_port);
      filter_ptr+= 17;
    } else {
      sprintf(filter_ptr, "%s%d", "dst port ", packet->dst_port);
      filter_ptr+= 13;
    }
  }

  rwlog_generate_bpf_filter(apply_filter);

  return;
}

static rw_status_t 
rwlog_rpc_apply_category_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter_Category) *category)
{
  uint32_t n_attributes;
  uint32_t index;
  rw_status_t status =  RW_STATUS_FAILURE;

  int cat= rwlogd_map_category_string_to_index(instance,category->name);
  if(cat == -1) {
    fprintf(stderr, "Invalid category name %s used.\n",category->name);
    return status;
  }

  if (category->n_attribute) {
    n_attributes = category->n_attribute;
    for (index = 0; index < n_attributes; index++) {
      status = 
          rwlogd_sink_filter_operation(FILTER_ADD,
                                       instance,
                                       category->name,
                                       sink_name, 
                                       category->attribute[index]->name, 
                                       category->attribute[index]->value,
                                       -1);
    }
  }

  if (category->has_severity)
  {
    status = rwlogd_sink_severity(instance, sink_name, 
                                  category->name,
                                  category->severity,
                                  category->has_critical_info? category->critical_info:RW_LOG_ON_OFF_TYPE_ON);
  }
  return status;
}

rw_status_t 
rwlog_rpc_apply_all_categories_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter_Category) *category)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  status = rwlog_rpc_apply_category_filter(instance, sink_name,category);
  if (status != RW_STATUS_SUCCESS) {
    return status;
  }
  if (category->packet) {
    rwlog_rpc_apply_packet_filter(instance, category->packet);
  }
  return status;
}


rw_status_t 
rwlog_apply_all_categories_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *category)
{
  rw_status_t status = RW_STATUS_FAILURE;
  status = rwlog_apply_category_filter(instance, sink_name,category);
  if (status != RW_STATUS_SUCCESS) {
    return status;
  }
  if (category->packet) {
    rwlog_apply_packet_filter(instance, category->packet);
  }
  return status;
}

rw_status_t 
rwlog_apply_next_call_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *config_filter) 
{
  rw_status_t status = RW_STATUS_FAILURE;

  if(config_filter->session_params && config_filter->has_next_call) {
    status = rwlog_apply_session_params_filter(instance,sink_name,config_filter->session_params,TRUE,config_filter->next_call);
  }
  else if(config_filter->has_next_failed_call) {
    status = rwlogd_sink_apply_failed_call_filter(instance, sink_name,config_filter->next_failed_call,TRUE);
  }
  else if (config_filter->has_next_call) {
    status = rwlogd_sink_apply_next_call_filter(instance, sink_name,config_filter->next_call);
  }
  return status;
}

rw_status_t 
rwlog_rpc_apply_failed_call_recording_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *config_filter) 
{
  rw_status_t status = RW_STATUS_FAILURE;

  if(config_filter->has_failed_call_recording) {
    status = rwlogd_sink_apply_failed_call_filter(instance, sink_name,config_filter->failed_call_recording,FALSE);
  }
  return status;
}

rw_status_t 
rwlog_apply_failed_call_recording_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *config_filter) 
{
  rw_status_t status = RW_STATUS_FAILURE;

  if(config_filter->has_failed_call_recording) {
    status = rwlogd_sink_apply_failed_call_filter(instance, sink_name,config_filter->failed_call_recording,FALSE);
  }
  return status;
}

rw_status_t 
rwlog_rpc_apply_session_params_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_SessionParams) *session_params,
                   bool has_next_call_flag,
                   RwLog__YangEnum__OnOffType__E next_call_flag)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  char category_str[] = "all";

  status =
      rwlogd_sink_filter_operation(FILTER_ADD,
                                   instance,
                                   category_str,
                                   sink_name, 
                                   "sess_param:imsi",
                                   session_params->imsi,
                                   has_next_call_flag?next_call_flag:-1);
  return status;
}

rw_status_t 
rwlog_apply_session_params_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_SessionParams) *session_params,
                   bool has_next_call_flag,
                   RwLog__YangEnum__OnOffType__E next_call_flag)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  char category_str[] = "all";

  status =
      rwlogd_sink_filter_operation(FILTER_ADD,
                                   instance,
                                   category_str,
                                   sink_name, 
                                   "sess_param:imsi",
                                   session_params->imsi,
                                   has_next_call_flag?next_call_flag:-1);
  return status;
}


void 
rwlog_apply_default_verbosity(rwlogd_instance_ptr_t instance, rwlog_severity_t sev, 
                              char *category_str)
{
  int cat= rwlogd_map_category_string_to_index(instance,category_str);
  if(cat == -1) {
    fprintf(stderr, "Invalid category name %s used.\n",category_str);
    return;
  }
  rwlogd_handle_default_severity(instance, category_str, sev);
}


static rw_status_t 
rwlog_apply_category_filter(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *category)
{
  uint32_t n_attributes;
  uint32_t index;
  rw_status_t status =  RW_STATUS_FAILURE;

  int cat= rwlogd_map_category_string_to_index(instance,category->name);
  if(cat == -1) {
    fprintf(stderr, "Invalid category name %s used.\n",category->name);
    return status;
  }

  if (category->n_attribute) {
    n_attributes = category->n_attribute;
    for (index = 0; index < n_attributes; index++) {
      status = 
          rwlogd_sink_filter_operation(FILTER_ADD,
                                       instance,
                                       category->name,
                                       sink_name, 
                                       category->attribute[index]->name, 
                                       category->attribute[index]->value,
                                       -1);
    }
  }

  if (category->has_severity)
  {
    status = rwlogd_sink_severity(instance, sink_name, 
                                  category->name,
                                  category->severity,
                                  category->has_critical_info? category->critical_info:RW_LOG_ON_OFF_TYPE_ON);
  }

  return status;
}

rw_status_t
rwlog_rpc_apply_filter_callgroup(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter)  *filter)
{
  rw_status_t status =  RW_STATUS_SUCCESS;

  if (filter->has_callid) {
        rwlogd_sink_filter_operation_uint64(FILTER_ADD,
                                   instance,
                                   "all",
                                   sink_name,
                                   "callid", filter->callid);
  }
  if (filter->has_groupcallid) {
        status =
        rwlogd_sink_filter_operation_uint64(FILTER_ADD,
                                   instance,
                                   "all", 
                                   sink_name, 
                                   "groupcallid", filter->groupcallid);
  }
  return (status);
}

rw_status_t
rwlog_apply_filter_callgroup(rwlogd_instance_ptr_t instance,
                   char *sink_name,
                   RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *filter)
{
  rw_status_t status =  RW_STATUS_SUCCESS;

  if (filter->has_callid) {
        rwlogd_sink_filter_operation_uint64(FILTER_ADD,
                                   instance,"all", sink_name,
                                   "callid", filter->callid);
  }
  if (filter->has_groupcallid) {
        status =
        rwlogd_sink_filter_operation_uint64(FILTER_ADD,
                                   instance,"all", sink_name, 
                                   "groupcallid", filter->groupcallid);
  }
  return (status);
}

static rw_status_t
rwlog_mgmt_rpc_config_filter(rwlogd_instance_ptr_t instance,
                          RWPB_T_MSG(RwBase_ReturnStatus) *ret_status,
                          const char *sink_name , RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *filter_config)
{
  uint32_t index;
  rw_status_t rw_status;
  RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *Filter = filter_config; 

  RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter_Category) **Category = Filter->category;
  
  if (filter_config->hostname) {
    char hostname[MAX_HOSTNAME_SZ];
    hostname[MAX_HOSTNAME_SZ-1]=0;
    gethostname(hostname,MAX_HOSTNAME_SZ-2);
    if (strncmp(hostname,filter_config->hostname, MAX_HOSTNAME_SZ-1)) {
      return (RW_STATUS_SUCCESS);
    } 
  }
  for (index = 0 ; index < Filter->n_category; index++) { 
    rw_status = rwlog_rpc_apply_all_categories_filter(instance, (char *)sink_name,  Category[index]);
    if (rw_status != RW_STATUS_SUCCESS ) {
      ret_status->has_error_number = TRUE;
      ret_status->error_number = -1;
      ret_status->has_error_string = TRUE;

      snprintf(ret_status->error_string, sizeof(ret_status->error_string),
               "RWLOGD-%d:Filter configuration failed for category %s",
               instance->rwtasklet_info->identity.rwtasklet_instance_id,
               Category[index]->name);
      return rw_status;
    }
  }

  if (Filter->deny ) {
   for (index = 0; index < Filter->deny->n_event; index++) {
     rw_status =
         rwlogd_shm_filter_operation_uint64(FILTER_DENY_EVID,
                                   instance, "event-id",
                                    Filter->deny->event[index]->event_id);
   }
  }    
  rw_status = rwlog_rpc_apply_filter_callgroup(instance, (char *)sink_name, Filter);
  if (rw_status != RW_STATUS_SUCCESS ) {
    ret_status->has_error_number = TRUE;
    ret_status->error_number = -1;
    ret_status->has_error_string = TRUE;
    snprintf(ret_status->error_string, sizeof(ret_status->error_string),
             "RWLOGD-%d:Filter configuration failed",
             instance->rwtasklet_info->identity.rwtasklet_instance_id);
  }

  if(Filter->has_protocol) {
    rwlogd_sink_update_protocol_filter(instance, (char *)sink_name, Filter->protocol,TRUE);
  }

  if(Filter->session_params) {
    rw_status = rwlog_rpc_apply_session_params_filter(instance, (char *)sink_name, Filter->session_params,FALSE,FALSE); 
    if (rw_status != RW_STATUS_SUCCESS ) {
      ret_status->has_error_number = TRUE;
      ret_status->error_number = -1;
      ret_status->has_error_string = TRUE;
      snprintf(ret_status->error_string, sizeof(ret_status->error_string),
               "RWLOGD-%d:Filter configuration failed",
               instance->rwtasklet_info->identity.rwtasklet_instance_id);
    }
  }

  if(Filter->has_failed_call_recording) {
    rw_status = rwlog_rpc_apply_failed_call_recording_filter(instance, (char *)sink_name, Filter); 
  }

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwlog_mgmt_config_filter(rwlogd_instance_ptr_t instance,
                          RWPB_T_MSG(RwBase_ReturnStatus) *ret_status,
                          const char *sink_name , RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *filter_config)
{
  uint32_t index;
  rw_status_t rw_status;
  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *Filter = filter_config; 

  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) **Category = Filter->category;

  for (index = 0 ; index < Filter->n_category; index++) { 
    rw_status = rwlog_apply_all_categories_filter(instance, (char *)sink_name,  Category[index]);
    if (rw_status != RW_STATUS_SUCCESS ) {
      ret_status->has_error_number = TRUE;
      ret_status->error_number = -1;
      ret_status->has_error_string = TRUE;

      snprintf(ret_status->error_string, sizeof(ret_status->error_string),
               "RWLOGD-%d:Filter configuration failed for category %s",
               instance->rwtasklet_info->identity.rwtasklet_instance_id,
               Category[index]->name);
      return rw_status;
    }
  }

  rw_status = rwlog_apply_filter_callgroup(instance, (char *)sink_name, Filter);
  if (rw_status != RW_STATUS_SUCCESS ) {
    ret_status->has_error_number = TRUE;
    ret_status->error_number = -1;
    ret_status->has_error_string = TRUE;
    snprintf(ret_status->error_string, sizeof(ret_status->error_string),
             "RWLOGD-%d:Filter configuration failed",
             instance->rwtasklet_info->identity.rwtasklet_instance_id);
  }

  if(Filter->has_protocol) {
    rw_status = rwlogd_sink_update_protocol_filter(instance,(char *)sink_name, Filter->protocol,TRUE);
  }

  if(Filter->session_params) {
    rw_status = rwlog_apply_session_params_filter(instance, (char *)sink_name, Filter->session_params,FALSE,FALSE); 
    if (rw_status != RW_STATUS_SUCCESS ) {
      ret_status->has_error_number = TRUE;
      ret_status->error_number = -1;
      ret_status->has_error_string = TRUE;
      snprintf(ret_status->error_string, sizeof(ret_status->error_string),
               "RWLOGD-%d:Filter configuration failed",
               instance->rwtasklet_info->identity.rwtasklet_instance_id);
    }
  }

  if(Filter->has_failed_call_recording) {
    rw_status = rwlog_apply_failed_call_recording_filter(instance, (char *)sink_name, Filter); 
  }

  return RW_STATUS_SUCCESS;
}

/* This funtion does zookeeper search rooted at top of collection and finds all
 * rwlogds.  rwvcs_rwzk_get_neighbors() will always return list in the same
 * order so we pick the first rwlog on it.
 * */
static int
rwlog_get_tasklet_instance_id(rwlogd_instance_ptr_t instance,char *tasklet_name)
{
  char *instance_name;
  RWPB_T_MSG(RwBase_ComponentInfo) component_info;
  uint32_t ret = 0,height = 0;
  rw_status_t status = RW_STATUS_SUCCESS;
  char ** neighbors;

  asprintf(&instance_name, "%s-%d", instance->rwtasklet_info->identity.rwtasklet_name, instance->rwtasklet_info->identity.rwtasklet_instance_id);
  if (!instance_name){
    return 0;
  }
  status = rwvcs_rwzk_lookup_component(instance->rwtasklet_info->rwvcs,
                                       instance_name,
                                       &component_info);

  for (height = 0; status == RW_STATUS_SUCCESS && component_info.rwcomponent_parent != NULL; ++height) {
    status = rwvcs_rwzk_lookup_component(instance->rwtasklet_info->rwvcs,
                                         component_info.rwcomponent_parent,
                                         &component_info);
  }

  status = rwvcs_rwzk_get_neighbors(instance->rwtasklet_info->rwvcs,
                                      instance_name,
                                      rwvcs_rwzk_test_component_name,
                                      tasklet_name,
                                      height, &neighbors);
  if(status == RW_STATUS_SUCCESS){
    if (neighbors[0]){
      int i = 0;
      ret = split_instance_id(neighbors[0]);
      for(i=0;neighbors[i] != NULL; i++) {
        free(neighbors[i]);
      }
      free(neighbors);
    }else{
      free(neighbors);
    }
  }
  free(instance_name);
  return ret;
}

static int
rwlog_get_cli_instance_id(rwlogd_instance_ptr_t instance)
{
  //return rwlog_get_tasklet_instance_id(instance,"RW.Cli");
  return 1;
}

static int 
rwlog_get_master_rwlogd_instance(rwlogd_instance_ptr_t instance)
{
  return rwlog_get_tasklet_instance_id(instance,"logd");
}


static rw_status_t
rwlog_apply_sink_config(rwlogd_instance_ptr_t instance, 
                        RWPB_T_MSG(RwlogMgmt_data_Logging_Sink) *Sink)
{
  RWPB_T_MSG(RwBase_ReturnStatus) ret_status;
  rw_status_t status = RW_STATUS_SUCCESS;

  if (Sink->server_address && Sink->has_port) {
      rwlogd_create_sink(rwlogd_syslog_sink, instance, Sink->name, Sink->server_address, Sink->port);
  }
  else if (Sink->filename) {
    uint32_t master_logd_id = rwlog_get_master_rwlogd_instance(instance);
    start_rwlogd_file_sink(instance, Sink->name, Sink->filename,master_logd_id);
  }
  else if (Sink->pcap_file) {
    start_rwlogd_pcap_sink(instance, Sink->name, Sink->pcap_file,0);
  }

  if(Sink->vnf_id) {
    rwlogd_sink_update_vnf_id(instance, Sink->name, Sink->vnf_id);
  }

  if (Sink && Sink->filter) {
    status = rwlog_mgmt_config_filter(instance, &ret_status, Sink->name, (void *)Sink->filter);
  }
  return status;
}

static rw_status_t 
rwlog_mgmt_config_console(rwlogd_instance_ptr_t instance,
                          RWPB_T_MSG(RwBase_ReturnStatus) *ret_status,
                          RWPB_T_MSG(RwlogMgmt_data_Logging_Console) *Console)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  if(Console->off) {
    stop_rwlogd_console_sink(instance,RWLOGD_CONSOLE_SINK_NAME);
    return RW_STATUS_SUCCESS;
  }
  if(Console->on) {
    start_rwlogd_console_sink(instance,RWLOGD_CONSOLE_SINK_NAME);
  }
  if(Console->vnf_id) {
    rwlogd_sink_update_vnf_id(instance, RWLOGD_CONSOLE_SINK_NAME, Console->vnf_id);
  }
  if(Console->filter) {
    status = rwlog_mgmt_config_filter(instance, ret_status, RWLOGD_CONSOLE_SINK_NAME, (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *)Console->filter);
  }
  return status;
}

static rw_status_t
rwlog_mgmt_delete_sink(rwlogd_instance_ptr_t instance, size_t n_sink, 
                      RWPB_T_MSG(RwBase_ReturnStatus) *ret_status,
                      RWPB_T_MSG(RwlogMgmt_data_Logging_Sink) **Sink)
{
  uint32_t index;
  rw_status_t rw_status = RW_STATUS_FAILURE;
    
  for (index = 0; index < n_sink; index ++) {
    ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
    protobuf_c_field_ref_goto_whole_message(&fref,(ProtobufCMessage*)Sink[index]);
    if (protobuf_c_field_ref_is_field_deleted(&fref)){
        rw_status = rwlogd_sink_delete(instance,Sink[index]->name);
    }
    if (rw_status != RW_STATUS_SUCCESS ) {
      ret_status->has_error_number = TRUE;
        ret_status->error_number = -1;
        ret_status->has_error_string = TRUE;
        snprintf(ret_status->error_string,
                  sizeof(ret_status->error_string),
                  "RWLOGD-%d:Sink configuration failed",
                  instance->rwtasklet_info->identity.rwtasklet_instance_id);

        return rw_status;
      }
    }
    return RW_STATUS_SUCCESS;
}


static rw_status_t
rwlog_mgmt_config_sink(rwlogd_instance_ptr_t instance, size_t n_sink, 
                      RWPB_T_MSG(RwBase_ReturnStatus) *ret_status,
                      RWPB_T_MSG(RwlogMgmt_data_Logging_Sink) **Sink)
{
  uint32_t index;
  rw_status_t rw_status;
    
    for (index = 0; index < n_sink; index ++) {
      rw_status = rwlog_apply_sink_config(instance,  Sink[index]);
      if (rw_status != RW_STATUS_SUCCESS ) {
        ret_status->has_error_number = TRUE;
        ret_status->error_number = -1;
        ret_status->has_error_string = TRUE;
        snprintf(ret_status->error_string,
                  sizeof(ret_status->error_string),
                  "RWLOGD-%d:Sink configuration failed",
                  instance->rwtasklet_info->identity.rwtasklet_instance_id);

        return rw_status;
      }
    }
    return RW_STATUS_SUCCESS;
}

static rwdts_member_rsp_code_t
rwlog_mgmt_handle_colony_request_dts (const rwdts_xact_info_t* xact_info,
                                      RWDtsQueryAction         action,
                                      const rw_keyspec_path_t*      key,
                                      const ProtobufCMessage*  msg,
                                      uint32_t credits,
                                      void *getnext_ptr)

{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  RWPB_T_MSG(RwlogMgmt_data_Logging) *Config;
  rwdts_member_rsp_code_t status = RWDTS_ACTION_OK;
  RWPB_T_MSG(RwBase_ReturnStatus) ret_status;


  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t) xact_info->ud;

  Config = (RWPB_T_MSG(RwlogMgmt_data_Logging) *) msg;
 
  if (!Config) {
    return status;
  }
 

  switch (action) {
    case RWDTS_QUERY_CREATE:
    case RWDTS_QUERY_UPDATE:
      if (Config->n_sink) {
        rw_status_t rs = rwlog_mgmt_config_sink(instance, Config->n_sink, &ret_status, Config->sink);
        if (rs != RW_STATUS_SUCCESS) {
          return RWDTS_ACTION_NOT_OK;
        }
      }
      if (Config->n_default_severity) {
        int index;
        for(index = 0;index < Config->n_default_severity;index++) { 
          if (Config->default_severity[index]->has_severity) 
          {

            rwlog_apply_default_verbosity(instance, Config->default_severity[index]->severity, 
                                          Config->default_severity[index]->category);
          }
        }
      }
      if (Config->deny ) {
        int index;
        for (index = 0; index < Config->deny->n_event; index++) {
            rwlogd_shm_filter_operation_uint64(FILTER_DENY_EVID,
                                       instance, "event-id",
                                       Config->deny->event[index]->event_id);
        }
      }    
      if (Config->allow) { /* Allow duplicate events */
       rwlog_allow_dup_events(instance); 
      }
      if(Config->console) {
        rw_status_t rs = rwlog_mgmt_config_console(instance, &ret_status, Config->console);
        if (rs != RW_STATUS_SUCCESS) {
          return RWDTS_ACTION_NOT_OK;
        }
      }
    
      break;
    case  RWDTS_QUERY_DELETE:
      if (Config->n_sink) {
        rw_status_t rs = rwlog_mgmt_delete_sink(instance, Config->n_sink, &ret_status, Config->sink);
        if (rs != RW_STATUS_SUCCESS) {
          return RWDTS_ACTION_NOT_OK;
        }
      }
      break;
    case RWDTS_QUERY_READ:
//      rwlog_handle_show_request_dts(xact,queryh,evt,key,instance,Config);
      break;
    default:
      return RWDTS_ACTION_NOT_OK;
  }
  return status;
}
int * rwlog_bytes_to_hex(const char *string, size_t length)
{
  int *hex = malloc(length * sizeof *hex);
  if(hex != NULL)
  {
    size_t i;
    for(i = 0; i < length; i++)
    {
      const char str = tolower(string[i]);
      hex[i] = (str <= '9') ? (str - '\0') : (10 + (str - 'a'));
    }
  }
  return hex;
}
static rwdts_member_rsp_code_t
rwlog_mgmt_handle_log_request_dts (const rwdts_xact_info_t* xact_info,
                                   RWDtsQueryAction         action,
                                   const rw_keyspec_path_t*      key,
                                   const ProtobufCMessage*  msg,
                                   uint32_t credits,
                                   void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_member_rsp_code_t status = RWDTS_ACTION_OK;
  RWPB_T_MSG(RwlogMgmt_input_LogEvent) *log_action;
  RWPB_T_MSG(RwBase_ReturnStatus) ret_status;
  char *name = NULL;
  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t) xact_info->ud;
  if (!msg) {
    return status;
  }
  int r  = asprintf (&name, "%s", "RW.CLI"); /* Only single CLI sink supported for now */
  RW_ASSERT(r);
  if (!r) {
    return RWDTS_ACTION_NOT_OK;
  }

  log_action = (RWPB_T_MSG(RwlogMgmt_input_LogEvent) *) msg;

  if (log_action->has_on && log_action->on) {
    uint32_t cli_instance_id = rwlog_get_cli_instance_id(instance);
    rwlogd_create_sink(rwlogd_cli_sink, instance, name, "RW.CLI", cli_instance_id);
  }
  if (log_action->has_off && log_action->off) {
    rwlogd_delete_sink(rwlogd_cli_sink, instance, name, "RW.CLI", 0);
  }
  if(log_action->vnf_id) {
    rwlogd_sink_update_vnf_id(instance, name, log_action->vnf_id);
  }
  if(log_action->filter && (log_action->filter->has_next_call || log_action->filter->has_next_failed_call)) {
    rwlog_apply_next_call_filter(instance,name,log_action->filter);
  }
  else if (log_action->filter) {
    rw_status_t rs = rwlog_mgmt_rpc_config_filter(instance, &ret_status, name, log_action->filter);
    if (rs != RW_STATUS_SUCCESS) {
      return RWDTS_ACTION_NOT_OK;
    }
  }
  if (log_action->has_checkpoint && log_action->checkpoint) {
        status = rwlogd_chkpt_logs(instance);
   }
  free(name);
  return status;
}

static void rwlogd_init_cli_output(rwlogd_output_specifier_t *out_spec)
{
  out_spec->verbosity = 0;
  out_spec->callid = 0;
  out_spec->callgroupid = 0;
  out_spec->start_time.tv_sec = 0;
  out_spec->start_time.tv_usec = 0;
  out_spec->end_time.tv_sec = 0;
  out_spec->end_time.tv_usec = 0;
  out_spec->log_line[0]= '\0';
  memset(&out_spec->msg_id, 0, sizeof(out_spec->msg_id));
  out_spec->has_input_msg_id= FALSE;
  out_spec->pdu_dump[0]= '\0';
  out_spec->pdu_hex_dump[0]= '\0';
  out_spec->show_filter_ptr = NULL;
  out_spec->has_inactive_logs = 0;
  out_spec->inactive_log_buffer = NULL;
  out_spec->inactive_log_size = 0;
}

static void
rw_log_copy_msg_id(RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) *dst,
                   rwlogd_output_specifier_t *out_spec)
{
  dst->has_tv_sec = 1;
  dst->has_tv_usec = 1;
  dst->has_hostname = 1;
  dst->has_process_id = 1;
  dst->has_thread_id  = 1;
  dst->has_seqno = 1;
  dst->tv_sec = out_spec->msg_id.tv_sec;
  dst->tv_usec = out_spec->msg_id.tv_usec;
  strcpy(dst->hostname,out_spec->msg_id.hostname); /* TBD strncpy */
  dst->process_id = out_spec->msg_id.pid;
  dst->thread_id = out_spec->msg_id.tid;
  dst->seqno = out_spec->msg_id.seqno;
}

rw_status_t rwlog_mgmt_fetch_logs(rwlogd_instance_ptr_t instance, 
                           RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input,
                           RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  RWPB_T_MSG(RwLog_output_ShowLogsInternal_Logs) *log_msgs;
  uint32_t i, empty_log_count = 0, logs_count = CLI_DEFAULT_LOG_LINES;
  size_t rsz;
  struct timeval start_t, end_t;
  rwlog_severity_t default_severity = RW_LOG_LOG_SEVERITY_DEBUG;
  rwlogd_output_specifier_t out_spec;
  rwlogd_init_cli_output(&out_spec);
  log->log_summary = RW_MALLOC(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal_LogSummary) *));
  log->n_logs = 0;

  RWPB_T_MSG(RwLog_output_ShowLogsInternal_LogSummary) *log_summary = 
      RW_MALLOC(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal_LogSummary)));

  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal_LogSummary,log_summary);

  if (!instance) {
    fprintf(stderr, "No Log instance ....\n");
    return RW_STATUS_FAILURE;
  }
  gettimeofday(&start_t, NULL);

  if( log_input->has_severity) {
    default_severity = log_input->severity;
  }

  out_spec.show_filter_ptr =  rwlogd_create_show_log_filter(instance,default_severity,log_input->filter);

  out_spec.cat = rwlogd_map_category_string_to_index(instance,"all");
  out_spec.verbosity=-1;

  //rwlogd_get_log_lock();

  if (log_input->has_verbosity) {
    out_spec.verbosity = log_input->verbosity;
  } 

  if (log_input->has_inactive) {
    out_spec.has_inactive_logs = true;
    out_spec.inactive_log_buffer = instance->rwlogd_info->log_buffer_chkpt;
    out_spec.inactive_log_size = instance->rwlogd_info->log_buffer_size;
  }

  const char *fmt = "%Y-%m-%dT%H:%M:%S";
  const char *usecfmt = ".%06uZ";
  char *usecstr;
  struct tm timep;

  if (log_input->start_time) {
    memset(&timep, 0, sizeof(timep));
    usecstr = strptime(log_input->start_time,fmt,&timep);
    out_spec.start_time.tv_sec = timegm(&timep);
    if (usecstr) {
      uint32_t u_sec;
      int retval =sscanf(usecstr,usecfmt,&u_sec);
      if (retval == 1) {
        out_spec.start_time.tv_usec = u_sec; 
      }
    }
  }
  if (log_input->end_time) {
    memset(&timep, 0, sizeof(timep));
    usecstr = strptime(log_input->end_time,fmt,&timep);
    out_spec.end_time.tv_sec = timegm(&timep);
    if (usecstr) {
      uint32_t u_sec;
      int retval =sscanf(usecstr,usecfmt,&u_sec);
      if (retval == 1) {
        out_spec.end_time.tv_usec = u_sec; 
      }
    }
  }

  memset(&out_spec.input_msg_id, 0, sizeof (out_spec.input_msg_id));
 

  if (log_input->has_tv_sec) {
    out_spec.input_msg_id.tv_sec = log_input->tv_sec;
    out_spec.has_input_msg_id = TRUE;
  }
  if (log_input->has_tv_usec) {
    out_spec.input_msg_id.tv_usec = log_input->tv_usec;
    out_spec.has_input_msg_id = TRUE;
  }
  if (log_input->has_hostname) {
    strcpy(out_spec.input_msg_id.hostname, log_input->hostname);
    out_spec.has_input_msg_id = TRUE;
  }
  if (log_input->has_process_id) {
    out_spec.input_msg_id.pid = log_input->process_id;
    out_spec.has_input_msg_id = TRUE;
  }
  if (log_input->has_thread_id) {
    out_spec.input_msg_id.tid = log_input->thread_id;
    out_spec.has_input_msg_id = TRUE;
  }
  if (log_input->has_seqno) {
    out_spec.input_msg_id.seqno = log_input->seqno;
    out_spec.has_input_msg_id = TRUE;
  }
   
  if (log_input->vnf_id) {
    uuid_parse(log_input->vnf_id,out_spec.vnf_id);
  }
  else {
    uuid_clear(out_spec.vnf_id);
  }

  if (log_input && log_input->filter) {
    if (log_input->filter->n_category == 1) {
      out_spec.cat = rwlogd_map_category_string_to_index(instance,log_input->filter->category[0]->name);
      if(out_spec.cat == -1) {
        fprintf(stderr, "ERROR: Invalid category name %s used. Using category all\n",log_input->filter->category[0]->name);
        out_spec.cat = 0;
      }
    }
    if (log_input->filter->has_callid) {
      out_spec.callid = log_input->filter->callid;
    }
    if (log_input->filter->has_groupcallid) {
      out_spec.callgroupid = log_input->filter->groupcallid;
    }
  }

  if (log_input->has_count) {
    logs_count = log_input->count;
  }  
  void *sink_obj  = (void *)rwlogd_prepare_sink(instance);
  int position = (log_input->has_tail)?rwlogd_get_last_log_position(sink_obj):0;

  if (out_spec.start_time.tv_sec && out_spec.start_time.tv_usec) {
    rwlogd_get_position(sink_obj, &position, &out_spec.start_time);
  }
  else if (out_spec.input_msg_id.tv_sec && out_spec.input_msg_id.tv_usec) {
    struct timeval starttime;
    starttime.tv_sec = out_spec.input_msg_id.tv_sec;
    starttime.tv_usec = out_spec.input_msg_id.tv_usec;
    rwlogd_get_position(sink_obj, &position, &starttime);
  }

  log->logs = malloc( sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal_Logs) *) * logs_count);
  RW_ASSERT(log->logs);
  if (!log->logs) {
    return RW_STATUS_FAILURE;
  }
  for (i=0; i < CIRCULAR_BUFFER_FACTOR * CLI_MAX_LOG_LINES && log->n_logs < logs_count && position != -1; i++, position+=(log_input->has_tail)?-1:1) {
    log_msgs = (RWPB_T_MSG(RwLog_output_ShowLogsInternal_Logs) *)RW_MALLOC(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal_Logs)));
    RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal_Logs,log_msgs);

    /* Reset log fields in out_spec before queying each log */
    out_spec.log_line[0]= '\0';

    //out_spec.msg_id[0]= '\0';
    out_spec.msg_id.tv_sec = 0;
    out_spec.msg_id.tv_usec = 0;
    out_spec.msg_id.hostname[0] = 0;
    out_spec.msg_id.pid = 0;
    out_spec.msg_id.tid = 0;
    out_spec.msg_id.seqno = 0;

    out_spec.pdu_dump[0]= '\0';
    out_spec.pdu_hex_dump[0]= '\0';

    rw_status_t status = rwlogd_get_log(instance, &position, &out_spec);
    
    if(status == RW_STATUS_FAILURE) {
      RWLOG_FILTER_DEBUG_PRINT(" END of LOGs \n");
      protobuf_free(log_msgs);
      break;
    }
    if(status == RW_STATUS_NO_RESPONSE) {
      RWLOG_FILTER_DEBUG_PRINT("Empty Log \n");
      empty_log_count++;
      protobuf_free(log_msgs);
      continue;
    }

    log_msgs->msg = RW_STRDUP(out_spec.log_line);

    if (log_input->verbosity != -1) {
      if (strlen(out_spec.pdu_dump) > 0) {
	      log_msgs->pdu_detail = RW_STRDUP(out_spec.pdu_dump);
      }
	    if (log_input->verbosity == 5) {
        if (strlen(out_spec.pdu_hex_dump) > 0) {
	        log_msgs->pdu_hex = RW_STRDUP(out_spec.pdu_hex_dump);
        }
      }
    }
    rw_log_copy_msg_id(log_msgs, &out_spec);

    log->n_logs++;
    log->logs[log->n_logs-1] = log_msgs;
  }
  rsz = log->n_logs * sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal_Logs) *);
  log->logs = realloc(log->logs, rsz);
  //rwlogd_releas_log_lock();

  // Finally, send the result.
  gettimeofday(&end_t, NULL);
  log_summary->log_count = log->n_logs;
  log_summary->has_log_count = log->n_logs;

  log_summary->time_spent = ((end_t.tv_sec - start_t.tv_sec) * 1000) + 
                    ((end_t.tv_usec - start_t.tv_usec)/1000);
  log_summary->has_time_spent = 1;

  int ttspos = -1;
  if(position != -1) {
    //ttspos = MIN(MAX(0, position - 1), CIRCULAR_BUFFER_FACTOR * CLI_MAX_LOG_LINES);
     ttspos = MAX(0, position - 1);
  }
  else if(!log_input->start_time){
    ttspos = rwlogd_get_last_log_position(sink_obj);
  }

  char trailing_timestamp[32];

  trailing_timestamp[0]='\0';

  if (ttspos != -1 && RW_STATUS_SUCCESS == rwlogd_get_log_trailing_timestamp(instance, ttspos, trailing_timestamp, sizeof(trailing_timestamp))
      && trailing_timestamp[0]) {
    log_summary->trailing_timestamp = RW_STRDUP(trailing_timestamp);
  }
  else if(log_input->start_time)  {
    log_summary->trailing_timestamp = RW_STRDUP(log_input->start_time);
  }
  else {
    /* Indicate current time */
    char tmstr[128];
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    tmstr[0]='\0';
    struct tm* tm = gmtime(&current_time.tv_sec);
    int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
    int bytes = snprintf(tmstr+tmstroff, sizeof(tmstr)-tmstroff, ".%06luZ", current_time.tv_usec);
    RW_ASSERT(bytes < (int)sizeof(tmstr) && bytes != -1);

    log_summary->trailing_timestamp = RW_STRDUP(tmstr);
  }

  /* fill the msg-id key from the last log as part of log summary */
  if (log->n_logs) {
    log_summary->has_tv_sec = 1;
    log_summary->tv_sec = log->logs[log->n_logs-1]->tv_sec;
    log_summary->has_tv_usec = 1; 
    log_summary->tv_usec = log->logs[log->n_logs-1]->tv_usec; 
    log_summary->has_process_id = 1; 
    log_summary->process_id = log->logs[log->n_logs-1]->process_id; 
    log_summary->has_thread_id = 1; 
    log_summary->thread_id = log->logs[log->n_logs-1]->thread_id; 
    log_summary->has_seqno = 1; 
    log_summary->seqno = log->logs[log->n_logs-1]->seqno; 
    log_summary->has_hostname =1;
    strncpy(log_summary->hostname,log->logs[log->n_logs-1]->hostname, 
            sizeof(log_summary->hostname)-1);
  }
  else {
    RW_ASSERT(gethostname(log_summary->hostname, MAX_HOSTNAME_SZ-1) == 0);
  }
  log_summary->logs_muted = empty_log_count;
  log_summary->has_logs_muted = 1;
  log->n_log_summary = 1;
  log->log_summary[0] = log_summary;

  if(out_spec.show_filter_ptr) {
    rwlogd_delete_show_log_filter(instance,out_spec.show_filter_ptr);
  }

  return status;
}

static rwdts_member_rsp_code_t rwlog_mgmt_handle_log_query_debug(const rwdts_xact_info_t* xact_info,
                                 RWDtsQueryAction         action,
                                 const rw_keyspec_path_t*      key,
                                 const ProtobufCMessage*  msg)
{
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage *msgs[1];
  uint32_t i = 0;
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *)msg;
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Debug) *log_input_debug = (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Debug) *)(((RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *)msg)->debug);
  /*TODO= Debug command should go to its own Keyspec */
  RWPB_T_MSG(RwlogMgmt_output_ShowLogs) *log;

  if(!log_input_debug) {
    return RWDTS_ACTION_OK;
  }

  if(log_input->has_hostname) {
    char hostname[MAX_HOSTNAME_SZ];
    gethostname(hostname, MAX_HOSTNAME_SZ-1);
    if(strncmp(log_input->hostname,hostname,MAX_HOSTNAME_SZ)) {
      return RWDTS_ACTION_OK;
    }
  }

  log = (RWPB_T_MSG(RwlogMgmt_output_ShowLogs) *)RW_MALLOC(sizeof(RWPB_T_MSG(RwlogMgmt_output_ShowLogs)));
  RWPB_F_MSG_INIT(RwlogMgmt_output_ShowLogs,log);
  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t) xact_info->ud;
  if(log_input_debug->has_filter_settings) 
  { 
    log->n_severity_output = instance->rwlogd_info->num_categories;
    category_str_t *cat_list = rwlogd_get_category_list(instance);
    rwlog_severity_t severity;
    uint32_t cat;
    log->severity_output = RW_MALLOC(log->n_severity_output*sizeof(void *));
    RW_ASSERT(log->severity_output); 
    if (!log->severity_output) {
      return RWDTS_ACTION_NOT_OK;
    }
    for (cat = 0; cat < log->n_severity_output; cat++) {
      log->severity_output[cat]=RW_MALLOC(sizeof(RWPB_T_MSG(RwlogMgmt_output_ShowLogs_SeverityOutput)));
      RW_ASSERT(log->severity_output[cat]);
      if (!log->severity_output[cat]) {
        return RWDTS_ACTION_NOT_OK;
      }
      RWPB_F_MSG_INIT(RwlogMgmt_output_ShowLogs_SeverityOutput,log->severity_output[cat]);
      severity = rwlogd_sink_get_severity(instance, cat);
      asprintf(&log->severity_output[cat]->severity_info, "%s:%s", cat_list[cat],seve[severity]);
      RW_ASSERT(log->severity_output[cat]->severity_info);
      if (!log->severity_output[cat]->severity_info) {
        return RWDTS_ACTION_NOT_OK;
      }
    }
  }

  if (log_input_debug->has_dump_task_info) {
    rwlogd_dump_task_info(instance->rwlogd_info, instance->rwtasklet_info->identity.rwtasklet_instance_id,log_input_debug->dump_task_info);
    log->n_logs = 0;
  }
  rw_keyspec_path_t *ks =
      (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(RwlogMgmt_output_ShowLogs);
  rsp.msgs = msgs;
  rsp.msgs[0] = (ProtobufCMessage *)log;
  rsp.n_msgs = 1;
  rsp.ks = ks;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;
  rw_status_t rs_status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(rs_status == RW_STATUS_SUCCESS);
  if(log->severity_output) {  
    for (i = 0; i < log->n_severity_output; i++) {
      RW_FREE( log->severity_output[i]->severity_info);
      RW_FREE(log->severity_output[i]);
    }
    RW_FREE(log->severity_output);
  }
  RW_FREE(log);
 
  return ((rs_status==RW_STATUS_SUCCESS)?RWDTS_ACTION_OK:RWDTS_ACTION_NOT_OK);
}

static rwdts_member_rsp_code_t
rwlog_mgmt_handle_get_category_list (const rwdts_xact_info_t* xact_info,
                                 RWDtsQueryAction         action,
                                 const rw_keyspec_path_t*      key,
                                 const ProtobufCMessage*  msg,
                                 uint32_t credits,
                                 void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage *resp_msg[1];
  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t) xact_info->ud;
  int i = 0;

  RWPB_T_MSG(RwlogMgmt_data_Logging_Categories) category;
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Categories,&category);
  rw_keyspec_path_t *ks = (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(RwlogMgmt_data_Logging_Categories);

  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }

  switch (action) {
    case RWDTS_QUERY_CREATE:
    case RWDTS_QUERY_UPDATE:
    case RWDTS_QUERY_DELETE:
      return RWDTS_ACTION_OK;
    case RWDTS_QUERY_READ:
      break;
    default:
      RW_CRASH();
  }
  

  memset (&rsp, 0, sizeof (rsp));
  rsp.n_msgs = 1;
  rsp.msgs = resp_msg;
  resp_msg[0] = (ProtobufCMessage *) &category;
  rsp.ks = ks;


  category.n_category = instance->rwlogd_info->num_categories;
  char **category_list = (char **)RW_MALLOC(sizeof(char *)*instance->rwlogd_info->num_categories);    
  category_str_t *cat_list = rwlogd_get_category_list(instance);
  for(i=0;i<instance->rwlogd_info->num_categories;i++) {
   category_list[i] = (char *)cat_list[i];
  }
  category.category = (char **)category_list;

  rsp.evtrsp = RWDTS_EVTRSP_ACK;
  rwdts_member_send_response (xact_info->xact, xact_info->queryh, &rsp);
  RW_FREE(category_list);
  return RWDTS_ACTION_OK;
}


static rwdts_member_rsp_code_t
rwlog_mgmt_handle_log_query_dts (const rwdts_xact_info_t* xact_info,
                                 RWDtsQueryAction         action,
                                 const rw_keyspec_path_t*      key,
                                 const ProtobufCMessage*  msg,
                                 uint32_t credits,
                                 void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  RW_ASSERT(msg);  
  if (!msg) {
    return RWDTS_ACTION_NOT_OK;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *)msg;

  /* If debug and hostname is not present; handle in this Rwlogd itself */
  if (log_input->debug && !log_input->has_hostname)  {
     rwlog_mgmt_handle_log_query_debug(xact_info,action,key,msg);
     return RWDTS_ACTION_OK;
  }

#ifdef MULTI_VM_LOG_DISPLAY
  rwlogd_fetch_logs_from_other_rwlogd(xact_info,action,key,msg);
  return RWDTS_ACTION_ASYNC;
#else
  rwlog_mgmt_handle_log_query_dts_internal(xact_info,action,key,msg);
#endif

  return RWDTS_ACTION_OK;
}

//LCOV_EXCL_START
static rwdts_member_rsp_code_t
rwlog_mgmt_handle_log_query_dts_internal (const rwdts_xact_info_t* xact_info,
                                 RWDtsQueryAction         action,
                                 const rw_keyspec_path_t*      key,
                                 const ProtobufCMessage*  msg,
                                 uint32_t credits,
                                 void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  RW_ASSERT(msg); 
  if (!msg) {
    return RWDTS_ACTION_NOT_OK;
  }

  rwdts_member_rsp_code_t status = RWDTS_ACTION_OK;
  ProtobufCMessage *msgs[1];
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *)msg;
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log;
  log = (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC(sizeof(RWPB_T_MSG(RwlogMgmt_output_ShowLogs)));
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal,log);
  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t) xact_info->ud;
  uint32_t i;
  uint32_t alloced_logs;
  rwdts_member_query_rsp_t rsp;
  rw_status_t rs_status;

  if (!instance) {
    fprintf(stderr, "No Log instance ....\n");
    return status;
  }

  if (log_input->debug)  {
     rwlog_mgmt_handle_log_query_debug(xact_info,action,key,msg);
     return RWDTS_ACTION_OK;
  }

  rs_status = rwlog_mgmt_fetch_logs (instance, log_input, log);

  alloced_logs = log->n_logs;

#ifdef MULTI_VM_LOG_DISPLAY
  rw_keyspec_path_t *ks =
      (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(RwLog_output_ShowLogsInternal);
#else
  rw_keyspec_path_t *ks =
      (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(RwlogMgmt_output_ShowLogs);
#endif

  rsp.msgs = msgs;
  rsp.msgs[0] = (ProtobufCMessage *)log;
  rsp.n_msgs = 1;
  rsp.ks = ks;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  rs_status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(rs_status == RW_STATUS_SUCCESS);

  for (i=0; i < alloced_logs ; i++) {
    if (log->logs && log->logs[i]) {
      if (log->logs[i]->pdu_hex) {
        RW_FREE(log->logs[i]->pdu_hex);
      }
      if (log->logs[i]->pdu_detail) {
        RW_FREE(log->logs[i]->pdu_detail);
      }
      if (log->logs[i]->msg) {
        RW_FREE(log->logs[i]->msg);
      }
      RW_FREE(log->logs[i]);
    }
  }
  if (log->logs) {
    RW_FREE(log->logs);
  }
  if (log->n_log_summary) {
    for(i =0;i<log->n_log_summary; i++) {
      RW_FREE(log->log_summary[i]->trailing_timestamp);
      RW_FREE(log->log_summary[i]);
    }
    RW_FREE(log->log_summary);
  }
  RW_FREE(log);
  if (rs_status != RW_STATUS_SUCCESS) {
    return RWDTS_ACTION_NOT_OK;
  }
  return status;
}


void rwlogd_add_node_to_list(rwlogd_instance_ptr_t instance, int peer_rwlogd_instance)
{
  void *t = NULL;
  rw_status_t status =  RW_SKLIST_LOOKUP_BY_KEY(&(instance->rwlogd_info->peer_rwlogd_list), &peer_rwlogd_instance, &t);
  if(status != RW_STATUS_SUCCESS) {
    rwlogd_peer_node_entry_t *peer_node_entry;
    peer_node_entry = (rwlogd_peer_node_entry_t *)RW_MALLOC0(sizeof(rwlogd_peer_node_entry_t));
    char *node_name = NULL;
    asprintf(&node_name,
             "/R/%s/%d",
             RWLOGD_PROC,
             peer_rwlogd_instance);
    peer_node_entry->rwtasklet_name = node_name;
    peer_node_entry->rwtasklet_instance_id = peer_rwlogd_instance;
    peer_node_entry->current_size = 0;
    status = RW_SKLIST_INSERT(&(instance->rwlogd_info->peer_rwlogd_list), peer_node_entry);

    if(instance->rwlogd_info->rwlogd_list_ring) {
      rwlogd_add_node_to_consistent_hash_ring(instance->rwlogd_info->rwlogd_list_ring,node_name);
    }
  }
}


static rwdts_member_rsp_code_t
rwlog_mgmt_lead_update_internal(rwdts_xact_t*           xact,    
                                   RWDtsQueryAction        action,
                                   rwdts_member_event_t    evt,     
                                   const rw_keyspec_path_t*     keyspec, 
                                   const ProtobufCMessage* msg,     
                                   void*                   ud)
{
  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t)ud;
  RWPB_T_MSG(RwLog_data_RwlogdInstance_TaskId) *dtsreq = (RWPB_T_MSG(RwLog_data_RwlogdInstance_TaskId) *)msg;
  //int i = 0;

  RW_ASSERT(instance);
  if (!instance) {
    return RWDTS_ACTION_NOT_OK;
  }
  if (dtsreq){
    RW_ASSERT(dtsreq->base.descriptor ==
              RWPB_G_MSG_PBCMD(RwLog_data_RwlogdInstance_TaskId));
    if (dtsreq->base.descriptor !=
              RWPB_G_MSG_PBCMD(RwLog_data_RwlogdInstance_TaskId)) {
      return RWDTS_ACTION_NOT_OK;
    }
  }
  
  switch (action) {
    case RWDTS_QUERY_CREATE:
    case RWDTS_QUERY_UPDATE:
      rwlogd_add_node_to_list(instance,dtsreq->id);
      break;
    case RWDTS_QUERY_DELETE:
       break;
    case RWDTS_QUERY_READ:
      break;
    default:
      RW_CRASH();
  }
  return RWDTS_ACTION_OK;
}


static rwdts_member_rsp_code_t
rwlog_mgmt_lead_update(const rwdts_xact_info_t*      xact_info,
                          RWDtsQueryAction              action,
                          const rw_keyspec_path_t*           keyspec, 
                          const ProtobufCMessage*       msg,
                          uint32_t credits,
                          void *getnext_ptr)
{
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  RW_ASSERT(xact_info);
  return rwlog_mgmt_lead_update_internal(xact_info->xact, action,
                                            xact_info->event, 
                                            keyspec, msg, xact_info->ud);
}

static void 
rwlog_mgmt_lead_cache_update(rwdts_member_reg_handle_t regh,
                                const rw_keyspec_path_t*       ks,
                                const ProtobufCMessage*   msg,
                                void*                     ctx)
{
  rwdts_member_rsp_code_t code;
    
  code = rwlog_mgmt_lead_update_internal(NULL, RWDTS_QUERY_CREATE,
                                            (rwdts_member_event_t)NULL,
                                            ks, msg, ctx);
  RW_ASSERT(code == RWDTS_ACTION_OK);
  if (code != RWDTS_ACTION_OK){
    RW_CRASH();
  }
  return;
}


static void 
rwlog_mgmt_subscribe_rwlogd_data_update(rwlogd_instance_ptr_t instance)
{
  rwdts_registration_t dts_info;
  rw_keyspec_path_t                     *keyspec = NULL;
  memset(&dts_info, 0, sizeof(dts_info));

  RWPB_T_PATHSPEC(RwLog_data_RwlogdInstance_TaskId) rwlogd_instance;
  rwlogd_instance = (*RWPB_G_PATHSPEC_VALUE(RwLog_data_RwlogdInstance_TaskId));
  keyspec = (rw_keyspec_path_t *)&rwlogd_instance;
  rw_keyspec_path_set_category (keyspec, NULL , RW_SCHEMA_CATEGORY_DATA);
  dts_info.keyspec = keyspec;
  dts_info.desc = RWPB_G_MSG_PBCMD(RwLog_data_RwlogdInstance_TaskId);
  dts_info.flags = RWDTS_FLAG_SUBSCRIBER | RWDTS_FLAG_CACHE;
  dts_info.callback.cb.prepare = rwlog_mgmt_lead_update;
  dts_info.callback.cb.reg_ready_old = rwlog_mgmt_lead_cache_update;
  dts_info.callback.ud = (void *)instance;

  rwdts_member_register(NULL, instance->dts_h,dts_info.keyspec,&dts_info.callback,dts_info.desc,dts_info.flags,NULL);
}

rw_status_t
rwlog_mgmt_lead_registration_complete(rwdts_member_reg_handle_t regh,
                                        const rw_keyspec_path_t*        keyspec,
                                        const ProtobufCMessage*    msg,
                                        void *ud)
{
  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t)ud;
  rw_status_t status = RW_STATUS_SUCCESS;

  RWPB_T_MSG(RwLog_data_RwlogdInstance_TaskId) dtsreq;
  RWPB_T_PATHENTRY(RwLog_data_RwlogdInstance_TaskId) path_entry;

  path_entry = (*RWPB_G_PATHENTRY_VALUE(RwLog_data_RwlogdInstance_TaskId));
  path_entry.has_key00 = 1;
  path_entry.key00.id = instance->rwtasklet_info->identity.rwtasklet_instance_id;

  RWPB_F_MSG_INIT(RwLog_data_RwlogdInstance_TaskId, &dtsreq);

  dtsreq.id = instance->rwtasklet_info->identity.rwtasklet_instance_id;

  status = rwdts_member_reg_handle_create_element(
            (rwdts_member_reg_handle_t)instance->lead_update_dts_member_handle,
            (rw_keyspec_entry_t *)&path_entry,&dtsreq.base,NULL);
                                            
  rwlog_mgmt_subscribe_rwlogd_data_update(instance);

  return status;
}

static void rwlog_dynamic_schema_registration_cb(
      void * app_instance,
      const int batch_size,
      rwdynschema_module_t * modules)
{
  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t) app_instance;

  instance->dynschema_app_state = RW_MGMT_SCHEMA_APPLICATION_STATE_WORKING;

  
  rw_status_t status = rwlogd_handle_dynamic_schema_update(instance,
                                                           batch_size,
                                                           modules);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  instance->dynschema_app_state = RW_MGMT_SCHEMA_APPLICATION_STATE_READY;
  return;
}

static rwdts_member_rsp_code_t rwlog_dynamic_schema_state_cb(const rwdts_xact_info_t* xact_info,
                                   RWDtsQueryAction         action,
                                   const rw_keyspec_path_t*      key,
                                   const ProtobufCMessage*  msg,
                                   uint32_t credits,
                                   void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwlogd_instance_ptr_t instance = (rwlogd_instance_ptr_t) xact_info->ud;
  rwdts_member_rsp_code_t status = RWDTS_ACTION_OK;
  rw_status_t rs_status;
  ProtobufCMessage *msgs[1];
  rwdts_member_query_rsp_t rsp;

  RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps) rwdynschmema_client_state;
  RWPB_F_MSG_INIT(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps,&rwdynschmema_client_state);

  RWPB_T_PATHSPEC(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps) pathspec;

  rwdynschmema_client_state.name = instance->instance_name;
  rwdynschmema_client_state.has_state = 1;
  rwdynschmema_client_state.state = instance->dynschema_app_state;
  rwdynschmema_client_state.app_type = RW_MGMT_SCHEMA_APPLICATION_TYPE_CLIENT;

  pathspec = (*RWPB_G_PATHSPEC_VALUE(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps));
  pathspec.dompath.path001.has_key00 = 1;
  pathspec.dompath.path001.key00.name = instance->instance_name;

  rsp.msgs = msgs;
  rsp.msgs[0] = (ProtobufCMessage *)&rwdynschmema_client_state;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)&pathspec;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  rs_status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(rs_status == RW_STATUS_SUCCESS);

  return status;
}

static rw_status_t
rwlog_dynamic_schema_registration(rwlogd_instance_ptr_t instance)
{
  rwdts_registration_t dts_info;
  rw_keyspec_path_t                     *keyspec = NULL;
  memset(&dts_info, 0, sizeof(dts_info));

  /* Register for client's state update keyspec. This is used to indicate to
   * dynamic schema driver if client is ready to receive new shcemas */
  RWPB_T_PATHSPEC(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps) dynschema_ks;
  dynschema_ks = (*RWPB_G_PATHSPEC_VALUE(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps));
  keyspec = (rw_keyspec_path_t *)&dynschema_ks;
  rw_keyspec_path_set_category (keyspec, NULL , RW_SCHEMA_CATEGORY_DATA);
  dynschema_ks.dompath.path001.has_key00 = 1; 
  dynschema_ks.dompath.path001.key00.name = instance->instance_name;

  dts_info.keyspec = keyspec;
  dts_info.desc = RWPB_G_MSG_PBCMD(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps);
  dts_info.flags = RWDTS_FLAG_PUBLISHER;
  dts_info.callback.cb.prepare   = (void *)rwlog_dynamic_schema_state_cb;
  dts_info.callback.ud = instance;
  rwdts_member_register(NULL, instance->dts_h,dts_info.keyspec,&dts_info.callback,dts_info.desc,dts_info.flags,NULL);


  /* Register for Dynamic schema updates */
  instance->dynschema_reg_handle = rwdynschema_instance_register(instance->dts_h,
                                                                 rwlog_dynamic_schema_registration_cb,
                                                                 instance->instance_name,
                                                                 RWDYNSCHEMA_APP_TYPE_OTHER,
                                                                 instance,
                                                                 NULL);

  return RW_STATUS_SUCCESS;
}

#define DTS_TBL(_a,_b,_c,_d)  \
    {.keyspec=(rw_keyspec_path_t *)&(_a),.desc=&(_b),.callback={.cb.prepare = (_c),},.flags=(_d)}                                   


static void rwlog_dts_registration_continue(rwlogd_instance_ptr_t instance) {
  int i;

  asprintf(&instance->instance_name, "%s-%d", instance->rwtasklet_info->identity.rwtasklet_name, 
           instance->rwtasklet_info->identity.rwtasklet_instance_id);
  rwlog_dynamic_schema_registration(instance);


  static rwdts_registration_t regns[] = {

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwlogMgmt_data_Logging)),       \
            (*RWPB_G_MSG_PBCMD(RwlogMgmt_data_Logging)), \
            rwlog_mgmt_handle_colony_request_dts, RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DELTA_READY), /* config colony event */ 

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwlogMgmt_data_Logging_Categories)),       \
            (*RWPB_G_MSG_PBCMD(RwlogMgmt_data_Logging_Categories)), \
            rwlog_mgmt_handle_get_category_list, RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST), /* config colony event */ 

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwlogMgmt_input_LogEvent)), \
            (*RWPB_G_MSG_PBCMD(RwlogMgmt_input_LogEvent)),\
            rwlog_mgmt_handle_log_request_dts, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED), /* Log enable-disable event */

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwlogMgmt_input_ShowLogs)),
            (*RWPB_G_MSG_PBCMD(RwlogMgmt_input_ShowLogs)),
            rwlog_mgmt_handle_log_query_dts, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED|RWDTS_XACT_FLAG_ANYCAST), /* Log request */

#ifdef MULTI_VM_LOG_DISPLAY
    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwLog_input_ShowLogsInternal)),
            (*RWPB_G_MSG_PBCMD(RwLog_input_ShowLogsInternal)),
            rwlog_mgmt_handle_log_query_dts_internal, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED), /* Log request */
#endif
  };
  
  
  for (i = 0; i < sizeof (regns) / sizeof (rwdts_registration_t); i++) {
    regns[i].callback.ud = instance;
    rw_keyspec_path_t* lks = NULL;
    rw_keyspec_path_create_dup(regns[i].keyspec, NULL, &lks);
    if (i ==0 ) {
      rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_CONFIG);
    } 
    else if (i ==1){
      rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);
    }else {
      rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_RPC_INPUT);
    }
    rwdts_member_register(NULL, instance->dts_h,lks, &regns[i].callback,
                          regns[i].desc, regns[i].flags, NULL);
    rw_keyspec_path_free(lks, NULL);
  }

  rw_status_t rs;
  rs = rwlog_sysmgmt_dts_registration(instance->rwtasklet_info, instance->dts_h);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  rwdts_registration_t dts_info;
  rw_keyspec_path_t                     *keyspec = NULL;
  memset(&dts_info, 0, sizeof(dts_info));

  RWPB_T_PATHSPEC(RwLog_data_RwlogdInstance_TaskId) rwlogd_instance;
  rwlogd_instance = (*RWPB_G_PATHSPEC_VALUE(RwLog_data_RwlogdInstance_TaskId));
  keyspec = (rw_keyspec_path_t *)&rwlogd_instance;
  rw_keyspec_path_set_category (keyspec, NULL , RW_SCHEMA_CATEGORY_DATA);
  rwlogd_instance.dompath.path001.has_key00 = 1; 
  rwlogd_instance.dompath.path001.key00.id = instance->rwtasklet_info->identity.rwtasklet_instance_id;
 
  dts_info.keyspec = keyspec;
  dts_info.desc = RWPB_G_MSG_PBCMD(RwLog_data_RwlogdInstance_TaskId);
  dts_info.flags = RWDTS_FLAG_PUBLISHER |RWDTS_FLAG_CACHE|RWDTS_FLAG_SHARED;
  dts_info.callback.cb.reg_ready_old   = (void *)rwlog_mgmt_lead_registration_complete;
  dts_info.callback.ud = instance;
  instance->lead_update_dts_member_handle = (void *)rwdts_member_register(NULL, instance->dts_h,dts_info.keyspec,&dts_info.callback,dts_info.desc,dts_info.flags,NULL);

  rwdts_api_set_ypbc_schema(instance->dts_h,
                              (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwlogMgmt));
}

static void rwlogd_dts_state_changed(rwdts_api_t *apih,
				     rwdts_state_t state,
				     void*         user_data) {
  /* Stub out, we don't wait... */
  switch (state) {
  case RW_DTS_STATE_INIT:
    /* Normal tasks should resume init from here, up to regn complete */
    rwlog_dts_registration_continue((rwlogd_instance_ptr_t)user_data);
    rwdts_api_set_state(apih, RW_DTS_STATE_REGN_COMPLETE);
    break;
  case RW_DTS_STATE_CONFIG:
    //rwdts_api_set_state(apih, RW_DTS_STATE_RUN);
    break;
  default:
    break;
  }
}

//LCOV_EXCL_STOP
rw_status_t
rwlog_dts_registration (rwlogd_instance_ptr_t instance)
{
  rwtasklet_info_ptr_t tasklet = instance->rwtasklet_info;  

  // get a DTS API handle
  instance->dts_h = rwdts_api_new (tasklet,(rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwlogMgmt),
                                   rwlogd_dts_state_changed, instance, NULL);
  RW_ASSERT(instance->dts_h);

  /* Actual registration invoked in rwlog_dts_registration_continue, after DTS init */

  return RW_STATUS_SUCCESS;
}

/*!
 * This function converts string form event msg-id into c structure
 * @param [in] msg_id_string - input string form msg-id
 * @param [out] msg_id - pointer to rwlog_msg_id_key_t structure
 * @result - RW_STATUS_SUCCESS upon successful parsing of message id
 *
 */
rw_status_t 
atoc_msgid(char *msg_id_string, rwlog_msg_id_key_t *msg_id)
{
  char tmp_id_string[RWLOGD_LOG_MSG_ID_LEN];
  char *msg_id_time, *msg_id_host, *msg_id_pid;
  char *msg_id_tid, *msg_id_seqno;

  if (msg_id_string == NULL)    { return (RW_STATUS_FAILURE); }
  if (msg_id_string[0] == '\0') { return (RW_STATUS_FAILURE);}
  
  strncpy(tmp_id_string, msg_id_string, RWLOGD_LOG_MSG_ID_LEN);
  msg_id_time=strtok(tmp_id_string,"/");
  msg_id_host=strtok(NULL,"/"); 
  msg_id_pid=strtok(NULL,"/");
  msg_id_tid=strtok(NULL,"/"); 
  msg_id_seqno=strtok(NULL,"/");

  if (!msg_id_time||!msg_id_host||!msg_id_pid||!msg_id_tid||!msg_id_seqno) {
    RWLOG_DEBUG_PRINT("msg-id input is incorrect format\n");
    return (RW_STATUS_FAILURE); 
  }
      
  if (msg_id_time[0] !='*')  {
    char *msg_id_tv_sec = strtok(msg_id_time,".");
    char *msg_id_tv_usec = strtok(NULL,".");
    if (msg_id_tv_sec) {
      msg_id->tv_sec = atoi(msg_id_tv_sec);
    }
    if (msg_id_tv_usec) {
      msg_id->tv_usec = atoi(msg_id_tv_usec);
    }
  }
  

  if (msg_id_host[0] != '*') {
    strncpy(msg_id->hostname, msg_id_host, sizeof(msg_id->hostname));
  }

  if (msg_id_pid[0] != '*') {
      msg_id->pid = (uint32_t)atoi(msg_id_pid);
  }

  if (msg_id_tid[0] != '*')  {
      msg_id->tid  = (uint32_t)atoi(msg_id_tid);
  }

  if (msg_id_seqno[0] != '*')  {
    msg_id->seqno = atoll(msg_id_seqno);
  }
  return (RW_STATUS_SUCCESS);
}
