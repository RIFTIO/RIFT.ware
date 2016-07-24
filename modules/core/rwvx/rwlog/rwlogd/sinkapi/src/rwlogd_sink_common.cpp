
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include "rwlogd_sink_common.hpp"
//broken?! #include <boost/format.hpp>
#include <tcpdump/tcpdump_export.h>
#include <time.h>
#include <dlfcn.h>
#include <boost/scoped_array.hpp>
#include "rw_namespace.h"
#include <uuid/uuid.h>


extern "C" 
{

  void rwlogd_clear_log_from_default_sink(rwlogd_instance_ptr_t inst)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
    rwlogd_sink *sink = obj->default_sink;
    if(sink) {
      sink->clear_log_from_ring();
    }
  }

  void rwlogd_notify_log(rwlogd_tasklet_instance_ptr_t inst_data, uint8_t *proto)
  {
    rwlogd_sink_data *rwlogd_sink_obj  = (rwlogd_sink_data *)inst_data->sink_data;
    rwlogd_sink_obj->handle_evlog(proto);
  }

  void rwlogd_add_callid_filter(rwlogd_tasklet_instance_ptr_t inst_data, uint8_t *proto) 
  {
    rwlogd_sink_data *rwlogd_sink_obj = (rwlogd_sink_data *)inst_data->sink_data;
    rwlog_hdr_t *log_hdr= (rwlog_hdr_t *)proto;
    char field[] = "callid";
    int count = 0;

    if (!rwlogd_sink_obj->sink_queue_) {
      return;
    }

    /* Need lock here ??? */
    for (auto &pe: *rwlogd_sink_obj->sink_queue_) 
    { 
      pe->add_generated_callid_filter(proto); 
    }

    if(log_hdr->cid.callid) {
      for (auto &pe: *rwlogd_sink_obj->sink_queue_)  {
        if(pe->get_filter_uint64(field, log_hdr->cid.callid)) {
          count++;
        }
      }
      if(count == 0) {
        return;
      }

      rwlogd_sink_obj->merge_filter_uint64(FILTER_ADD, (uint32_t)RW_LOG_LOG_CATEGORY_ALL, field, log_hdr->cid.callid);
    }

    if(log_hdr->cid_filter.cid_attrs.next_call_callid) {
      rwlogd_notify_log(inst_data,proto);
      /* Remove next-call filter config in shared memory */
      rwlogd_sink_obj->merge_next_call_filter();
    }

    if(log_hdr->cid_filter.cid_attrs.failed_call_callid) {
      rwlogd_notify_log(inst_data,proto);
      /* Remove next-call filter config in shared memory */
      rwlogd_sink_obj->merge_failed_call_filter();
    }

  }


  void rwlogd_remove_callid_filter(rwlogd_tasklet_instance_ptr_t inst_data, uint64_t callid) 
  {
    rwlogd_sink_data *rwlogd_sink_obj = (rwlogd_sink_data *)inst_data->sink_data;
    int count = 0;
    char field[] = "callid";

    if (!rwlogd_sink_obj->sink_queue_) {
      return;
    }


    if(callid) {
      for (auto &pe: *rwlogd_sink_obj->sink_queue_)  {
        if(pe->get_filter_uint64(field, callid)) {
          count++;
        }
      }
      if(count == 0) {
        return;
      }

      /* Need lock here ??? */
      for (auto &pe: *rwlogd_sink_obj->sink_queue_) 
      { 
        pe->remove_generated_callid_filter(callid); 
      }

      rwlogd_sink_obj->merge_filter_uint64(FILTER_DEL, (uint32_t)RW_LOG_LOG_CATEGORY_ALL, field, callid);
    }
  }


  void rwlogd_handle_file_log(rwlogd_tasklet_instance_ptr_t inst_data, char *log_string,char *name)
  {
    rwlogd_sink_data *rwlogd_sink_obj  = (rwlogd_sink_data *)inst_data->sink_data;
    if (!rwlogd_sink_obj->sink_queue_) {
      return;
    }
    rwlogd_sink *sink = rwlogd_sink_obj->lookup_sink(name);
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", RWLOGD_DEFAULT_SINK_NAME);
      return;
    }
    sink->handle_file_log(log_string);
  }


  void rwlogd_handle_file_rotation(rwlogd_tasklet_instance_ptr_t inst_data)
  {
    rwlogd_sink_data *rwlogd_sink_obj  = (rwlogd_sink_data *)inst_data->sink_data;
    rwlogd_sink_obj->handle_file_rotation();
  }
  void rwlogd_shm_reset_bootstrap(rwlogd_tasklet_instance_ptr_t inst_data)
  {
    rwlogd_sink_data *rwlogd_sink_obj  = (rwlogd_sink_data *)inst_data->sink_data;
    rwlogd_sink_obj->shm_reset_bootstrap();
  }

  void rwlogd_shm_incr_ticks(rwlogd_tasklet_instance_ptr_t inst_data)
  {
    rwlogd_sink_data *rwlogd_sink_obj  = (rwlogd_sink_data *)inst_data->sink_data;
    rwlogd_sink_obj->shm_incr_ticks();
  }

  void rwlogd_shm_set_flowctl(rwlogd_tasklet_instance_ptr_t inst_data)
  {
    rwlogd_sink_data *rwlogd_sink_obj  = (rwlogd_sink_data *)inst_data->sink_data;
    rwlogd_sink_obj->shm_set_flow();
  }

  void rwlogd_shm_reset_flowctl(rwlogd_tasklet_instance_ptr_t inst_data)
  {
    rwlogd_sink_data *rwlogd_sink_obj  = (rwlogd_sink_data *)inst_data->sink_data;
    rwlogd_sink_obj->shm_reset_flow();
  }

  void rwlogd_handle_default_severity(rwlogd_instance_ptr_t rwlogd_instance, 
                                      char *category_str,
                                      rwlog_severity_t sev)
  {
    rwlogd_tasklet_instance_ptr_t inst_data = rwlogd_instance->rwlogd_info;
    rwlogd_sink_data *rwlogd_sink_obj  = (rwlogd_sink_data *)inst_data->sink_data;
    int category = rwlogd_sink_obj->map_category_string_to_index(category_str);
    if(category < 0) {
      return;
    }
    rwlogd_sink_obj->set_severity(category, sev);
  }

  void rwlogd_sink_load_schema(rwlogd_tasklet_instance_ptr_t rwlogd_instance_data,  const char *schema_name)
  {
    rwlogd_sink_data *rwlogd_sink_obj = (rwlogd_sink_data *)rwlogd_instance_data->sink_data;
    if(schema_name) {
      rwlogd_sink_obj->load_log_yang_modules(schema_name);
      rwlogd_sink_obj->update_category_list();
      //rwlogd_instance_data->num_categories =  rwlogd_sink_obj->num_categories_;
    }
    return;
  }

  void rwlogd_sink_obj_init(rwlogd_tasklet_instance_ptr_t rwlogd_instance_data,char *filter_shm_name,
                            const char *schema_name)
  {
    rwlogd_instance_data->sink_data = new rwlogd_sink_data(rwlogd_instance_data,filter_shm_name, schema_name);
    RW_ASSERT(rwlogd_instance_data->sink_data);
    if (!rwlogd_instance_data->sink_data) {
      return;
    }

    RWLOG_DEBUG_PRINT("rwlogd_sink_obj_init *** created %p %p\n", 
                      rwlogd_instance_data,
                      rwlogd_instance_data->sink_data);
  }

  void rwlogd_sink_obj_deinit(rwlogd_tasklet_instance_ptr_t rwlogd_instance_data)
  {     
    delete (rwlogd_sink_data *)rwlogd_instance_data->sink_data;
    RWLOG_DEBUG_PRINT("rwlogd_sink_obj_init *** destroyed\n");
  } 
  rwlogd_sink_data *rwlogd_get_sink_obj (rwlogd_instance_ptr_t rwlogd_instance)
  {
    rwlogd_tasklet_instance_ptr_t inst_data = rwlogd_instance->rwlogd_info;
    return (rwlogd_sink_data *)inst_data->sink_data;
  }
  void rwlogd_dump_task_info(rwlogd_tasklet_instance_ptr_t info, int instance_id, int verbosity)
  {
    printf("rwlogd instance data\n");
    printf("---------------------\n");
    printf("Instance id: %d\n",instance_id);
    printf("Log invalid magic: %lu\n",info->stats.log_invalid_magic);
    printf("Log Header Validation Failed: %lu\n",info->stats.log_hdr_validation_failed);
    printf("Circular buffer wrap count: %lu\n",info->stats.circ_buff_wrap_around);
    printf("Circular buffer wrap failure: %lu\n",info->stats.circ_buff_wrap_failure);
    printf("File rotations: %lu\n",info->stats.file_rotations);
    printf("Log checkpoint stats : %lu\n",info->stats.chkpt_stats);
    printf("Logs forwarded to peer: %lu\n",info->stats.log_forwarded_to_peer);
    printf("Logs received from peer: %lu\n",info->stats.logs_received_from_peer);
    printf("Invalid Log from peer: %lu\n",info->stats.invalid_log_from_peer);
    printf("Sending log to peer Failed: %lu\n",info->stats.sending_log_to_peer_failed);
    printf("Send Log req to peer: %lu\n",info->stats.peer_send_requests);
    printf("Received Log req from peer: %lu\n",info->stats.peer_recv_requests);
    printf("Multiple File Rotations Attempted: %lu\n",info->stats.multiple_file_rotations_atttempted);
    printf("\n");

    printf("rwlogd node list\n");
    printf("----------------\n");
    if(info->rwlogd_list_ring) {
      rwlogd_display_consistent_hash_ring(info->rwlogd_list_ring);
    }
    printf("\n");
    fflush(stdout);

    ((rwlogd_sink_data *)info->sink_data)->rwlog_dump_info(verbosity);
    fflush(stdout);
  }
  rw_status_t rwlogd_get_log(rwlogd_instance_ptr_t inst,
                             int *location,
                             rwlogd_output_specifier_t *output_spec) 
  {
    int position = *location;
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
    rwlogd_sink *sink = obj->lookup_sink(RWLOGD_DEFAULT_SINK_NAME);
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", RWLOGD_DEFAULT_SINK_NAME);
      return RW_STATUS_FAILURE;
    }
    return sink->get_log(position,output_spec); 
  }
  void *rwlogd_prepare_sink(rwlogd_instance_ptr_t inst)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
    rwlogd_sink *sink = obj->lookup_sink(RWLOGD_DEFAULT_SINK_NAME);
    return (void *)sink;
  }

  int rwlogd_get_last_log_position(void *sink_obj) 
  {
    rwlogd_sink *sink = (rwlogd_sink *)sink_obj;
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", RWLOGD_DEFAULT_SINK_NAME);
      return RW_STATUS_FAILURE;
    }

    return  sink->get_last_position();
  }

  rw_status_t rwlogd_get_position(void *sink_obj,
                                  int *location, 
                                  struct timeval *start_time) 
  {
    rwlogd_sink *sink = (rwlogd_sink *)sink_obj;
    int position = 0;
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", RWLOGD_DEFAULT_SINK_NAME);
      *location = -1;
      return RW_STATUS_FAILURE;
    }
    rw_status_t status = sink->get_position(position,start_time);
    if (RW_STATUS_SUCCESS == status) {
      *location = position;
    } 
    else {
      *location = -1;
    }

    return (status);
  }

  rw_status_t rwlogd_get_log_trailing_timestamp(rwlogd_instance_ptr_t inst,
                                                int position,
                                                char *buf,
                                                size_t buf_sz) {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
    rwlogd_sink *sink = obj->lookup_sink(RWLOGD_DEFAULT_SINK_NAME);
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", RWLOGD_DEFAULT_SINK_NAME);
      return RW_STATUS_FAILURE;
    }
    return sink->get_log_trailing_timestamp(position, buf, buf_sz);
  }

  void rwlogd_set_tcpdump_buf(int verbosity)
  {
    rwtcpdump_state_t *rwtcpdump_state;
    if ((rwtcpdump_state = 
         (rwtcpdump_state_t *)pthread_getspecific(log_print_key)) == NULL) {
      rwtcpdump_state = (rwtcpdump_state_t *)RW_MALLOC0(sizeof(rwtcpdump_state_t));
      pthread_setspecific(log_print_key, (void*) rwtcpdump_state);
    }
    rwtcpdump_state->verbosity = verbosity;
    return;
  }

  void rwlogd_reset_tcpdump_buf(int verbosity)
  {
    rwtcpdump_state_t *rwtcpdump_state;
    if ((rwtcpdump_state = 
         (rwtcpdump_state_t *)pthread_getspecific(log_print_key)) != NULL) {
      memset((char *)rwtcpdump_state, 0, sizeof(rwtcpdump_state_t));
      rwtcpdump_state->verbosity = verbosity;
    }
    return;
  }

  char *rwlogd_get_tcpdump_buf()
  {
    rwtcpdump_state_t *rwtcpdump_state;
    if ((rwtcpdump_state = (rwtcpdump_state_t *)pthread_getspecific(log_print_key)) != NULL) {
      return rwtcpdump_state->tcpdump_out_buf;
    } else {
      return NULL;
    }
  }

  void rwlog_update_pkt_info(rw_pkt_info_t *pkt_info, pkt_trace_t *tmp_pkt_info)
  {
    if (tmp_pkt_info->has_packet_type) {
      pkt_info->packet_type = tmp_pkt_info->packet_type;
    }
    if (tmp_pkt_info->has_packet_direction) {
      pkt_info->packet_direction = tmp_pkt_info->packet_direction;
    }
    if (tmp_pkt_info->has_sport) {
      pkt_info->sport = tmp_pkt_info->sport;
    }
    if (tmp_pkt_info->has_dport) {
      pkt_info->dport = tmp_pkt_info->dport;
    }
    if (tmp_pkt_info->has_ip_header) {
      pkt_info->ip_header = tmp_pkt_info->ip_header;
    }
    if (tmp_pkt_info->has_fragment) {
      pkt_info->fragment = tmp_pkt_info->fragment;
    }
    return;
  }

  /*******************************************************************************
   *  rwlogd_shm_filter_operation
   *    FILTER_ACTION : ADD, REMOVE, MODIFY,LOOKUP
   *  @params:  action
   *  @params:  instance
   *  @params:  field_name
   *  @params: value: Should be set for A,R,M action type
   ******************************************************************************/
  rw_status_t rwlogd_shm_filter_operation_uint64(FILTER_ACTION action,
                                                 rwlogd_instance_ptr_t instance,
                                                 char *field_name,
                                                 uint64_t value)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);

    if (action == FILTER_DENY_EVID) {
      obj->merge_filter_uint64(action, RW_LOG_LOG_CATEGORY_ALL, field_name, value);
      obj->incr_log_serial_no();
    }
    return(RW_STATUS_SUCCESS);
  }

  /*!
   * disables duplicate events suppression feature 
   */
  rw_status_t rwlog_allow_dup_events( rwlogd_instance_ptr_t instance)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    obj->set_dup_events_flag(TRUE);
    return (RW_STATUS_SUCCESS);
  }
  /*******************************************************************************
   *  rwlogd_sink_filter_operation 
   *    Handle all filter related operations
   *    Calls the filter action in the SINK objs
   *    Calls the merging of share filters.
   *    FILTER_ACTION : ADD, REMOVE, MODIFY,LOOKUP
   *  @params:  action
   *  @params:  instance
   *  @params:  name
   *  @params:  field_name
   *  @params: value: Should be set for A,R,M action type
   ******************************************************************************/
  rw_status_t rwlogd_sink_filter_operation(FILTER_ACTION action,
                                           rwlogd_instance_ptr_t instance, 
                                           char *category,
                                           char *name, 
                                           char *field_name, 
                                           char *value,
                                           int next_call_flag)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    rwlogd_sink *sink = obj->lookup_sink(name);
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", name);
      return RW_STATUS_FAILURE;
    }
    rw_status_t status = RW_STATUS_SUCCESS;
    if ((action != FILTER_LKUP || action != FILTER_DENY_EVID))
    {
      {
        obj->merge_filter(action, category, field_name, value);
        obj->incr_log_serial_no();
        status = sink->filter_operation(action, category, 
                                        field_name,value,next_call_flag);
      }
    }
    return status;
  }
  /*******************************************************************************
   *  rwlogd_sink_filter_operation_uint64
   *    Handle filter related operations for callid/groupcallid
   *    Calls the filter action in the SINK objs
   *    Calls the merging of share filters.
   *    FILTER_ACTION : ADD, REMOVE, MODIFY,LOOKUP
   *  @params:  action
   *  @params:  instance
   *  @params:  name
   *  @params:  field_name
   *  @params: value: Should be set for A,R,M action type
   ******************************************************************************/
  rw_status_t rwlogd_sink_filter_operation_uint64(FILTER_ACTION action,
                                                  rwlogd_instance_ptr_t instance, 
                                                  char *category,
                                                  char *name, 
                                                  char *field_name, 
                                                  uint64_t value)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    rwlogd_sink *sink = obj->lookup_sink(name);

    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", name);
      return RW_STATUS_FAILURE;
    }

    rw_status_t status = sink->filter_uint64(action,field_name,value);
    uint32_t category_index = (uint32_t)obj->map_category_string_to_index(category);

    if (status == RW_STATUS_SUCCESS && action != FILTER_LKUP)
    {
      obj->merge_filter_uint64(action, category_index, field_name, value);
      obj->incr_log_serial_no();
    }
    return status;
  }

  rw_status_t rwlogd_sink_apply_failed_call_filter(rwlogd_instance_ptr_t instance, 
                                                   char *sink_name,
                                                   RwLog__YangEnum__OnOffType__E next_failed_call_flag,
                                                   bool next_call_filter)
  {
    rw_status_t status = RW_STATUS_SUCCESS;
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    rwlogd_sink *sink = obj->lookup_sink(sink_name);
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", sink_name);
      return RW_STATUS_FAILURE;
    }

    bool enable  = (next_failed_call_flag == RW_LOG_ON_OFF_TYPE_ON)?TRUE:FALSE;
    sink->update_failed_call_filter(enable,next_call_filter);

    obj->merge_failed_call_filter();

    return status;
  }

  rw_status_t rwlogd_sink_apply_next_call_filter(rwlogd_instance_ptr_t instance, 
                                                 char *sink_name,
                                                 RwLog__YangEnum__OnOffType__E next_call_flag)
  {
    rw_status_t status = RW_STATUS_SUCCESS;
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    rwlogd_sink *sink = obj->lookup_sink(sink_name);
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", sink_name);
      return RW_STATUS_FAILURE;
    }

    bool enable  = (next_call_flag == RW_LOG_ON_OFF_TYPE_ON)?TRUE:FALSE;
    sink->update_next_call_filter(enable);

    obj->merge_next_call_filter();

    return status;
  }

  uint32_t
  rwlogd_get_num_categories(rwlogd_instance_ptr_t instance)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    RW_ASSERT(obj);
    if (!obj) { return 0; }
    return obj->num_categories_;
  }

  category_str_t *
  rwlogd_get_category_list(rwlogd_instance_ptr_t instance)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    RW_ASSERT(obj);
    if (!obj) { return 0; }
  
    return obj->category_list_;
  }

  int
  rwlogd_map_category_string_to_index(rwlogd_instance_ptr_t instance,
                                      char *category_str)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    RW_ASSERT(obj);
    if (!obj) { return 0; }

    return obj->map_category_string_to_index(category_str);
  }

  rw_status_t rwlogd_sink_severity(rwlogd_instance_ptr_t instance, 
                                   char *sink_name,
                                   char *category_str,
                                   rwlog_severity_t severity,
                                   RwLog__YangEnum__OnOffType__E critical_info_filter)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    rwlogd_sink *sink = obj->lookup_sink(sink_name);
    int category_index = -1;
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", sink_name);
      return RW_STATUS_FAILURE;
    }
    category_index = obj->map_category_string_to_index(category_str);
    if(category_index < 0) {
      RWLOG_DEBUG_PRINT("Unknown category found %s\n", category_str);
      return RW_STATUS_FAILURE;
    }

    rw_status_t status = sink->set_severity(category_index, severity);
    if (status == RW_STATUS_SUCCESS)
    {
      if(category_index) {
        obj->merge_severity(FILTER_ADD, category_index, severity);
      }
      else {
        for(uint32_t index = 0; index < obj->num_categories_;index++) {
          obj->merge_severity(FILTER_ADD, index, severity);
        }
      }
    }
    sink->sink_filter_.set_critical_info_filter(category_index,critical_info_filter);
    return status;
  }


  rw_status_t rwlogd_sink_update_protocol_filter(rwlogd_instance_ptr_t instance,
                                                 char *sink_name,
                                                 RwLog__YangEnum__ProtocolType__E protocol,
                                                 bool enable)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    rwlogd_sink *sink = obj->lookup_sink(sink_name);
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", sink_name);
      return RW_STATUS_FAILURE;
    }
    sink->set_protocol_filter((uint32_t)protocol,enable);

    obj->merge_protocol_filter(protocol);

    return RW_STATUS_SUCCESS;
  }

  rwlog_severity_t rwlogd_sink_get_severity(rwlogd_instance_ptr_t instance, 
                                            uint32_t category)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    if (!obj) {
      RWLOG_DEBUG_PRINT("Sink Data not found\n");
      return RW_LOG_LOG_SEVERITY_EMERGENCY;
    }
    return (obj->get_severity(category));
  }


  rw_status_t
  rwlogd_sink_delete(rwlogd_instance_ptr_t instance,
                            char *sink_name)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    rwlogd_sink *sink = obj->remove_sink(sink_name);
    if (sink)
    {
      delete (sink);
    }
    return RW_STATUS_SUCCESS;
  }

  rw_status_t
  rwlogd_sink_update_vnf_id(rwlogd_instance_ptr_t instance,
                            char *sink_name,
                            char *vnf_id) 
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj (instance);
    rwlogd_sink *sink = obj->lookup_sink(sink_name);
    if (!sink)
    {
      RWLOG_DEBUG_PRINT("Sink not found %s\n", sink_name);
      return RW_STATUS_FAILURE;
    }
    sink->update_vnf_id(vnf_id);
    return RW_STATUS_SUCCESS;
  }

  rw_status_t
  rwlogd_handle_dynamic_schema_update(rwlogd_instance_ptr_t instance,
                                      const uint64_t batch_size,
                                      rwdynschema_module_t *modules)
  {
    rwlogd_sink_data *obj = rwlogd_get_sink_obj(instance);
    RW_ASSERT(obj);
    if (!obj) { return RW_STATUS_FAILURE; }

    rwsched_tasklet_t * tinfo = instance->rwtasklet_info->rwsched_tasklet_info;
    rwsched_dispatch_queue_t queue = rwsched_dispatch_queue_create(
        tinfo,
        "schema loding queue",
        RWSCHED_DISPATCH_QUEUE_SERIAL);
    obj->dynamic_schema_update(tinfo,
                               queue,
                               batch_size,
                               modules);
    
    instance->rwlogd_info->num_categories = obj->num_categories_;
    return RW_STATUS_SUCCESS;
  }

}
// Initialisers
rw_yang::YangModel* rwlogd_sink_data::yang_model_ = NULL;
msg_descr_cache_t* rwlogd_sink_data::msg_descr_cache_ = NULL;
rwlogd_sink_data::sink_data_stats rwlogd_sink_data::stats;
uint32_t rwlogd_sink_data::num_categories_ = 0;
category_str_t  *rwlogd_sink_data::category_list_ = NULL;


static inline std::string c_escape_string(
    const char* input,
    unsigned len)
{
  // Maximumun possible length. All characters could be non-printable plus 1
  // byte for null termination.
  unsigned dest_len = (len * 4) + 1;
  boost::scoped_array<char> dest(new char[dest_len]);
  RW_ASSERT(dest);
  if (!dest) {
    return NULL;
  }
  unsigned used = 0;
  for (unsigned i = 0; i < len; i++) {
    switch(input[i]) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n'; break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r'; break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't'; break;
      case '\"': dest[used++] = '\\'; dest[used++] = '\"'; break;
      case '\'': dest[used++] = '\\'; dest[used++] = '\''; break;
      case '\\': dest[used++] = '\\'; dest[used++] = '\\'; break;
      default: {
        if (!isprint(input[i])) {
          char oct[10];
          // octal escaping for non printable characters.
          sprintf(oct, "\\%03o", static_cast<uint8_t>(input[i]));
          for (unsigned j = 0; j < 4; j++) {
            dest[used++] = oct[j];
          }
        } else {
          dest[used++] = input[i];
        }
      }
    }
  }
  RW_ASSERT(used <= dest_len);
  dest[used] = '\0';
  return std::string(dest.get());
}



/*******************************************************************************
        Class: rwlogd_sink_data 
        ========================
        Exists as void* in instance data
        Maintains All the Sink queues. 
        Maintains All Eventlog circular buffer for the queus
******************************************************************************/
rwlogd_sink_data::rwlogd_sink_data(rwlogd_tasklet_instance_ptr_t rwlogd_info, char *filter_shm_name, const char* schema_name)
{
  sink_queue_ = new sink_queue_t();
  category_list_ = (category_str_t *)RW_MALLOC0(RWLOG_MAX_NUM_CATEGORIES * sizeof(category_str_t));
  num_categories_ = 0;
  rwlogd_info_ = rwlogd_info;

  if(schema_name) {
    load_log_yang_modules(schema_name);
    get_category_name_list();
  }
  else {
    /* Have category "all" as first category */
    strncpy(category_list_[num_categories_],"all",sizeof(category_str_t));
    num_categories_++;
  }

  rwlogd_filter_init(filter_shm_name);
  msg_descr_cache_ = new msg_descr_cache_t(RWLOGS_MAX_CACHE_DESCRIPTORS);
  pthread_key_create(&log_print_key, free);
  gndo->ndo_default_print = rwtcpdump_ndo_default_print;
  gndo->ndo_printf = rwtcpdump_printf;

  memset(&rwlogd_sink_data::stats, 0, sizeof(rwlogd_sink_data::stats));
  RWLOG_DEBUG_PRINT ("rwlogd_sink_data::rwlogd_sink_data %p\n", this);
}

void rwlogd_sink_data::rwlog_dump_info(int verbose)
{
  printf("rwlogd_sink_data info\n");
  printf("---------------------\n");
  stats.rwlog_dump_info(verbose);
  msg_descr_cache_->print();
  shm_buffer_dump_free_blk();
  rwlog_app_filter_dump((void *)rwlogd_shm_ctrl_);
  printf ("last_location %d\n", rwlogd_shm_ctrl_->last_location);

  int i = 0;
  for (auto &pe: *sink_queue_)
  {
    i++;
    printf ("****** Sink %d *********\n",i); 
    pe->rwlog_dump_info(verbose);
  }
  printf("***********************\n");
}

rwlogd_sink_data::~rwlogd_sink_data()
{
  RWLOG_DEBUG_PRINT ("Del::rwlogd_sink_data %p\n", this);
  if(yang_model_)
    delete yang_model_;
  yang_model_ = NULL;
  if(sink_queue_)
    delete sink_queue_;
  if(msg_descr_cache_)
    delete msg_descr_cache_;
  msg_descr_cache_ = NULL;
  if(rwlogd_filter_memory_) {
    munmap(rwlogd_filter_memory_,RWLOG_FILTER_SHM_SIZE);
  }
  shm_unlink(rwlog_shm_name_);
  close(filter_shm_fd_);
  RW_FREE(rwlog_shm_name_);
}
 
// SINK APIS
rw_status_t rwlogd_sink_data::add_sink(rwlogd_sink *snk)
{
  if (!snk)
  {
    RW_CRASH();
  }
  if (!sink_queue_)
  {
    RW_CRASH();
  }
  sink_queue_->push_front(snk);
  snk->set_backptr(this);
  snk->create_category_list();
  return RW_STATUS_SUCCESS;
}

rwlogd_sink *rwlogd_sink_data::remove_sink(const char *name)
{
  rwlogd_sink *sink = lookup_sink(name);
  if(sink)
  {
    sink_queue_->remove(sink);
  }
  return sink;
}

rwlogd_sink *rwlogd_sink_data::lookup_sink(const char *name)
{
  for (auto &pe: *sink_queue_) 
  {
    if(strcmp(pe->get_name(),name) == 0)
      return pe;
  }
  return NULL;
}

rw_status_t rwlogd_sink_data::handle_evlog(uint8_t *buffer)
{
  if (!sink_queue_)
    return RW_STATUS_FAILURE;

  for (auto &pe: *sink_queue_) 
  { 
    if (pe->get_state()!= CONNECTED) 
    { 
      RWLOG_DEBUG_PRINT ("Too Early Call.......\n");
      continue;
    }
    pe->handle_evlog(buffer); 
  }
  return RW_STATUS_SUCCESS;
}

void rwlogd_sink_data::enable_L1_filter()
{
  rwlogd_filter_memory_->skip_L1 = FALSE;
}

void rwlogd_sink_data::handle_file_rotation()
{
  rwlogd_filter_memory_->rotation_serial++;
}

void rwlogd_sink_data::shm_reset_bootstrap()
{
  rwlogd_filter_memory_->skip_L1 = FALSE;
  rwlogd_filter_memory_->skip_L2 = FALSE;
  rwlogd_filter_memory_->bootstrap = FALSE;
  rwlogd_filter_memory_->allow_dup = FALSE;
  incr_log_serial_no();

  RWLOG_DEBUG_PRINT("shm reseting bootstrap procedure\n");
}

void rwlogd_sink_data::rwlogd_filter_init(char *filter_shm_name)
{
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;

  filter_info = NULL;
  deny_filter_info = NULL;
  protocol_filter_set_ = FALSE;

  if(filter_shm_name) {
    rwlog_shm_name_ = RW_STRDUP(filter_shm_name);
  }
  else {
    int r = asprintf (&rwlog_shm_name_,
                      "%s-%d",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId());
    RW_ASSERT(r);
    if (!r) { return; }
  }

  filter_shm_fd_ =  shm_open(rwlog_shm_name_,oflags,perms);
  if (filter_shm_fd_ < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM:%s\n", strerror(errno), rwlog_shm_name_);
    return;
  }

  ftruncate(filter_shm_fd_, RWLOG_FILTER_SHM_SIZE);
  rwlogd_shm_ctrl_ =
      (rwlogd_shm_ctrl_t *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd_, 0);
  if (MAP_FAILED == rwlogd_shm_ctrl_)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for MAP_FAILED SHM:%s\n", strerror(errno), rwlog_shm_name_);
    return;
  }


  //Header
  //memset(rwlogd_shm_ctrl_, 0, sizeof(rwlogd_shm_ctrl_t));
  rwlogd_filter_memory_ = &rwlogd_shm_ctrl_->hdr;
  rwlogd_filter_memory_->rwlogd_pid = getpid();
  rwlogd_filter_memory_->rotation_serial = 0;
  rwlogd_filter_memory_->static_bitmap =  0;
  rwlogd_filter_memory_->allow_dup = FALSE;

  /* Configure to pass all L1 and L2 filters for the moment.  A very
     temporary situation.  We actually want a "hidden" command of
     sorts to enable either or both of these at whim for testing etc.
     Disabling L1 in particular tests marshalling of all events. */

  /* Setting skip_L1 and skip_L2 to TRUE and they will be reset after bootstrap
   * period */
  rwlogd_filter_memory_->skip_L1 = TRUE;
  rwlogd_filter_memory_->skip_L2 = TRUE;
  rwlogd_filter_memory_->bootstrap = TRUE;

  rwlogd_filter_memory_->rwlogd_flow = FALSE;
  rwlogd_filter_memory_->log_serial_no = 1;

  rwlogd_filter_memory_->num_categories = num_categories_;
  RW_ASSERT(num_categories_ < RWLOG_MAX_NUM_CATEGORIES);
  if (num_categories_ >= RWLOG_MAX_NUM_CATEGORIES) {
    num_categories_ = RWLOG_MAX_NUM_CATEGORIES-1;
  }
  rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)rwlogd_filter_memory_ + sizeof(rwlogd_shm_ctrl_t)); 
  rwlogd_filter_memory_->category_offset = sizeof(rwlogd_shm_ctrl_t);
  severity_ = (rwlog_severity_t *)RW_MALLOC0(RWLOG_MAX_NUM_CATEGORIES * sizeof(rwlog_severity_t));
  for(uint32_t i = 0;i < num_categories_; i++) {
    memcpy((char *)category_filter[i].name,(char *)category_list_[i], sizeof(category_str_t));
    category_filter[i].severity = RW_LOG_LOG_SEVERITY_ERROR;
    category_filter[i].bitmap = 0;
    category_filter[i].critical_info = RW_LOG_ON_OFF_TYPE_ON;
    severity_[i] = RW_LOG_LOG_SEVERITY_ERROR;
  }
  category_filter_ = category_filter;
  rwlogd_shm_ctrl_->last_location = sizeof(rwlogd_shm_ctrl_t) + RWLOG_MAX_NUM_CATEGORIES*sizeof(rwlog_category_filter_t);

  rwlogd_filter_memory_->magic = RWLOG_FILTER_MAGIC;  // Set filter last so that we have valid data */
}

/****************************************************************************
 * rwlogd_sink_data::merge_filter 
 * Appends data to Shared Memory
 *
 * @params : Action . Add,del, lkup
 * @params : category . Yang module name
 * @params : Feild and value. 
 * Note: Field is ignored 
 ***************************************************************************/
void rwlogd_sink_data::merge_filter(FILTER_ACTION action,
                                    char *cat_str,
                                    char *field, 
                                    char *value)
{
  uint64_t hash_index = 0;
  int is_set = 0;
  if (!field || !value)
  {
    RWLOG_ERROR_PRINT("merge_filters:Invalid parameters");
    return;
  }
  int cat = map_category_string_to_index(cat_str);
  if(cat < 0) {
    return;
  }
  char *field_value_combo=NULL;
  int r = asprintf (&field_value_combo, 
                    "%s:%s:%s",
                    field,
                    value,
                    cat_str);
  RW_ASSERT(r);
  if (!r) { return;}


  if (action == FILTER_ADD)
  {
    add_exact_match_filter_string(&filter_info,cat_str, field,value,-1); 
    BLOOM_SET(category_filter_[cat].bitmap,value);
    RWLOG_FILTER_DEBUG_PRINT("Add bit and bitmap is %lx\n", rwlogd_filter_memory_->bitmap[cat]);

    // New code
    hash_index = BLOOM_IS_SET(category_filter_[cat].bitmap,field_value_combo, is_set);
    is_set = is_set;
    filter_array_hdr *flt_ar_hdr = &rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
    int old_n_entries = flt_ar_hdr->n_entries;
    uint32_t new_hash_value_offset = l2_filter_add(flt_ar_hdr, field_value_combo);
    flt_ar_hdr->table_offset = (new_hash_value_offset)&0xfffff;
    flt_ar_hdr->n_entries = (old_n_entries+1)&0xfff;
    // 
    RWLOG_FILTER_DEBUG_PRINT("Added bit and bitmap is %lx hash_idx = %lu offset =%d \n", 
                             category_filter_[cat].bitmap, hash_index, 
                             rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE].table_offset);
  }
  if (action == FILTER_DEL)
  {
    bool clear_bloom = true;
    remove_exact_match_filter_string(&filter_info,cat_str, field,value);
    // New code
    hash_index = BLOOM_IS_SET(category_filter_[cat].bitmap,field_value_combo, is_set);
    is_set = is_set;
    filter_array_hdr *flt_ar_hdr = &rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
    int old_n_entries = flt_ar_hdr->n_entries;
    uint32_t new_hash_value_offset = l2_filter_remove(flt_ar_hdr,field_value_combo, clear_bloom);
    if(new_hash_value_offset)
    {
      flt_ar_hdr->table_offset = (new_hash_value_offset)&0xfffff;
      flt_ar_hdr->n_entries = (old_n_entries-1)&0xfff;
    }
    else {
      RW_DEBUG_ASSERT(flt_ar_hdr->n_entries == 1);
      flt_ar_hdr->table_offset = 0;
      flt_ar_hdr->n_entries = 0;
    }
     
    // Reference count 
    if (clear_bloom)
    {
      BLOOM_RESET(category_filter_[cat].bitmap,value);
    }
    RWLOG_FILTER_DEBUG_PRINT("Deleted bit and bitmap is %lx Clear Bloom %d\n", category_filter_[cat].bitmap, clear_bloom);
  }
  if (action == FILTER_DENY_EVID)
  {
    uint64_t eventId;
    char value_string[256];
    sscanf(value, "%lu",&eventId);
    snprintf(value_string,256,"event-id:%lu",eventId);
    add_exact_match_deny_filter_string(&deny_filter_info,field,value_string); 
    RWLOG_FILTER_DEBUG_PRINT("Add bit and bitmap is %lx\n", rwlogd_filter_memory_->static_bitmap);
    RWLOG_SET_DENY_ID_HASH(rwlogd_filter_memory_->static_bitmap,eventId);
    RWLOG_FILTER_DEBUG_PRINT("Added bit and bitmap is %lx\n", rwlogd_filter_memory_->static_bitmap);

    // New code
    hash_index = BLOOM_IS_SET(category_filter_[cat].bitmap,value_string, is_set);
    is_set = is_set;
    filter_array_hdr *flt_ar_hdr = &rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
    int old_n_entries = flt_ar_hdr->n_entries;
    uint32_t new_hash_value_offset = l2_filter_add(flt_ar_hdr, value_string);
    flt_ar_hdr->table_offset = (new_hash_value_offset)&0xfffff;
    flt_ar_hdr->n_entries = (old_n_entries+1)&0xfff;
    // 
  }
  if(field_value_combo)
  {
    free(field_value_combo);
  }
}

void rwlogd_sink_data::shm_buffer_free(uint32_t block_offset, 
                                       int freed_size,
                                       rwlogd_shm_bkt_sz bucket)
{
  rwlog_free_mem_hdr_t *blk_hdr = (rwlog_free_mem_hdr_t*)((uint64_t)rwlogd_filter_memory_ + block_offset);
  blk_hdr->size= freed_size;
  shm_free_list.push_back((uint32_t)(block_offset));
  shm_free_list.sort();
  RWLOG_FILTER_DEBUG_PRINT("shm_buffer_free %d\n", block_offset);
  //shm_buffer_dump_free_blk();
  // Coalesce . Maybe later in garbage collector
}
void rwlogd_sink_data::shm_buffer_dump_free_blk()
{
  std::list<uint32_t>::iterator it;
  printf("SHM free blocks ********\n");
  printf("%-10s %-10s\n", "offset", "size");
  for (it=shm_free_list.begin(); it!=shm_free_list.end(); ++it)
  {
    rwlog_free_mem_hdr_t *blk_hdr = (rwlog_free_mem_hdr_t*)((uint64_t)rwlogd_filter_memory_ + *it);
    printf("%-10d %-10d\n", *it, blk_hdr->size);
  }
}

uint32_t rwlogd_sink_data::shm_buffer_alloc(int request_size , rwlogd_shm_bkt_sz bucket)
{
  uint32_t block_offset = 0;
  std::list<uint32_t>::iterator it;
  RWLOG_FILTER_DEBUG_PRINT("ENTER shm_buffer_alloc\n");
  //shm_buffer_dump_free_blk();
  for (it=shm_free_list.begin(); it!=shm_free_list.end(); ++it)
  {
    block_offset = *it;
    rwlog_free_mem_hdr_t *blk_hdr = (rwlog_free_mem_hdr_t*)((uint64_t)rwlogd_filter_memory_ + block_offset);
    if (blk_hdr->size == request_size)
    {
      RWLOG_FILTER_DEBUG_PRINT("shm_buffer_alloc::Match:Offset Returned is %d req %d  blk_size %d\n", block_offset, request_size, blk_hdr->size);
      shm_free_list.remove(block_offset);
      //shm_buffer_dump_free_blk();
      return block_offset;
    }
    if (blk_hdr->size > request_size)
    {
      shm_free_list.insert(it, (block_offset + request_size));
      rwlog_free_mem_hdr_t *new_blk = (rwlog_free_mem_hdr_t *)((uint64_t)rwlogd_filter_memory_ + block_offset + request_size);
      new_blk->size = blk_hdr->size-request_size;
      RWLOG_FILTER_DEBUG_PRINT("shm_buffer_alloc::Smaller:Offset Returned is %d req %d  blk_size %d\n", block_offset, request_size, blk_hdr->size);
      shm_free_list.remove(block_offset);
      //shm_buffer_dump_free_blk();
      return block_offset;
    }
  }
  block_offset = rwlogd_shm_ctrl_->last_location;
  rwlogd_shm_ctrl_->last_location+=request_size;
  RWLOG_FILTER_DEBUG_PRINT("shm_buffer_alloc::Offset Returned is %d request_size %d\n", block_offset, request_size);
  //shm_buffer_dump_free_blk();
  return block_offset;
}
/****************************************************************************
 * Constructs L2 filter
 * Removes the old memory 
 * Allocates and copies to new memory
 * Returns the new_offse 
 * Caller replaces only the offset
 ***************************************************************************/

uint32_t rwlogd_sink_data::l2_filter_add(filter_array_hdr *old_hdr,
                                         char *value)
{
  uint32_t new_hash_value_offset = 0;
  char *new_flt_str_loc = NULL;
  char *old_flt_str_loc= NULL;
  int n_elements = 0;
  if (old_hdr->table_offset)
  {
    old_flt_str_loc = (char *)rwlogd_filter_memory_ + old_hdr->table_offset;
    n_elements = old_hdr->n_entries;
    if (!n_elements)
    {
      RWLOG_ERROR_PRINT("Inconsistent Data written as num_filters\n");
      return old_hdr->table_offset;
    }
    new_hash_value_offset = 
        shm_buffer_alloc((n_elements+1)*RWLOG_FILTER_STRING_SIZE, RWLOGD_SHM_FILTER_SIZE_256);
    new_flt_str_loc = (char *)rwlogd_filter_memory_ + new_hash_value_offset;

    bool exceeded = false;
    for(int src=0, dst=0;
        src<n_elements; 
        src++, dst++)
    {
      int rs = 0;
      if(!exceeded)
      {
        rs = strncmp((char *)value, old_flt_str_loc+src*RWLOG_FILTER_STRING_SIZE, RWLOG_FILTER_STRING_SIZE);
        if (rs >= 0)
        {
          exceeded=true;
          strlcpy(new_flt_str_loc+dst*RWLOG_FILTER_STRING_SIZE, (char *)value, RWLOG_FILTER_STRING_SIZE);
          dst++;
        }
      }
      strlcpy(new_flt_str_loc+dst*RWLOG_FILTER_STRING_SIZE, 
              old_flt_str_loc+src*RWLOG_FILTER_STRING_SIZE,
              RWLOG_FILTER_STRING_SIZE);
      //Slow,But source will get one filter
    }
    if(!exceeded)
    {
      strlcpy(new_flt_str_loc+n_elements*RWLOG_FILTER_STRING_SIZE, (char *)value, RWLOG_FILTER_STRING_SIZE);
    }
    shm_buffer_free(old_hdr->table_offset, n_elements*RWLOG_FILTER_STRING_SIZE, RWLOGD_SHM_FILTER_SIZE_256);
  }
  else
  {
    new_hash_value_offset = shm_buffer_alloc(RWLOG_FILTER_STRING_SIZE, RWLOGD_SHM_FILTER_SIZE_256);
    new_flt_str_loc = (char *)rwlogd_filter_memory_ + new_hash_value_offset;
    strlcpy(new_flt_str_loc, (char *)value, RWLOG_FILTER_STRING_SIZE);
  }
  return new_hash_value_offset;
}
/****************************************************************************
 * Constructs L2 filter
 * Removes the old memory 
 * Allocates and copies to new memory
 * Returns the new_offse 
 * Caller replaces only the offset
 ***************************************************************************/
uint32_t rwlogd_sink_data::l2_filter_remove(filter_array_hdr *flt_hdr,
                                            char *value, 
                                            bool &clear_bloom)
{
  uint32_t new_hash_value_offset = 0;
  char *new_flt_str_loc = NULL;
  char *old_flt_str_loc = NULL;
  int n_elements = 0;
  clear_bloom=true;
  if (flt_hdr->table_offset)
  {
    old_flt_str_loc = (char *)rwlogd_filter_memory_ + flt_hdr->table_offset;
    n_elements = flt_hdr->n_entries;
    if (!n_elements)
    {
      RWLOG_ERROR_PRINT("Inconsistent Data written as num_filters\n");
      return 0;
    }
    new_hash_value_offset = shm_buffer_alloc((n_elements-1)*RWLOG_FILTER_STRING_SIZE, RWLOGD_SHM_FILTER_SIZE_256);
    new_flt_str_loc = (char *)rwlogd_filter_memory_ + new_hash_value_offset;

    bool found = false;
    for(int src=0, dst=0;
        src<n_elements; 
        src++, dst++)
    {
      int rs = 0;
      if(!found)
      {
        rs = strncmp((char *)value, old_flt_str_loc+src*RWLOG_FILTER_STRING_SIZE, RWLOG_FILTER_STRING_SIZE);
        if (rs == 0)
        {
          found=true;
          src++;
        }
        if(src<n_elements)
        {
          rs = strncmp((char *)value, old_flt_str_loc+src*RWLOG_FILTER_STRING_SIZE, RWLOG_FILTER_STRING_SIZE);
          if(rs == 0)
          {
            clear_bloom = false;
          }
        }
      }
      if(n_elements == 1)
      {
        shm_buffer_free(new_hash_value_offset, RWLOG_FILTER_STRING_SIZE, RWLOGD_SHM_FILTER_SIZE_256);
        new_hash_value_offset = 0;
        break;
      }
      strlcpy(new_flt_str_loc+dst*RWLOG_FILTER_STRING_SIZE, 
              old_flt_str_loc+src*RWLOG_FILTER_STRING_SIZE,
              RWLOG_FILTER_STRING_SIZE);
      //Slow, But source will get one filter
    } 
    shm_buffer_free(flt_hdr->table_offset, n_elements*RWLOG_FILTER_STRING_SIZE, RWLOGD_SHM_FILTER_SIZE_256);
  }
  else
  {
    RWLOG_ERROR_PRINT("No Filter Set\n");
  }
  return new_hash_value_offset;
}

void rwlogd_sink_data::set_dup_events_flag(bool flag)
{
  rwlogd_filter_memory_->allow_dup = flag;
}

/****************************************************************************
 * rwlogd_sink_data::merge_filter 
 * If action is add and the severity is higher than the CLI input,
 *    Severity is reduced
 * If action is del and severity is lower than the CLI input , 
 *    Severity is increased to the lowest of all sinks.
 *
 * @params : Action . Add,del, lkup
 * @params : category . Yang module name
 * @params : Feild and value. 
 * Note: Field is ignored 
 ***************************************************************************/
void rwlogd_sink_data::merge_filter_uint64(FILTER_ACTION action,
                                           uint32_t cat,
                                           char * field, 
                                           uint64_t value)
{
  uint64_t hash_index = 0;
  int is_set = 0;
  int count = 0;
  if (!field || !value)
  {
    RWLOG_ERROR_PRINT("merge_filter_uint64:Invalid parameters");
    return;
  }

  for (auto &pe: *sink_queue_)  {
    if(pe->get_filter_uint64(field, value)) {
      count++;
    }
  }
  if (count >1) {
    return;
  }

  char *field_value_combo=NULL;
  int r = asprintf (&field_value_combo, "%s:%lu", field, value);
  RW_ASSERT(r);
  if (!r) { return; }

  if (action == FILTER_ADD)
  {
    add_exact_match_filter_uint64(&filter_info,field,value); 
    RWLOG_FILTER_DEBUG_PRINT("Add bit and bitmap is %lx\n", category_filter_[cat].bitmap);

    BLOOM_SET_UINT64(category_filter_[cat].bitmap,&value);
    RWLOG_FILTER_DEBUG_PRINT("Added bit and bitmap is %lx\n", category_filter_[cat].bitmap);
    // New code
    hash_index = BLOOM_IS_SET(category_filter_[0].bitmap,field_value_combo, is_set);
    is_set = is_set;
    filter_array_hdr *flt_ar_hdr = &rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
    int old_n_entries = flt_ar_hdr->n_entries;
    uint32_t new_hash_value_offset = l2_filter_add(flt_ar_hdr, field_value_combo);
    flt_ar_hdr->table_offset = (new_hash_value_offset)&0xfffff;
    flt_ar_hdr->n_entries = (old_n_entries+1)&0xfff;
    //
    RWLOG_FILTER_DEBUG_PRINT("Added bit and bitmap is %lx hash_idx = %lu offset =%d \n",
                             category_filter_[cat].bitmap, hash_index,
                             rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE].table_offset);
  }
  if (action == FILTER_DEL)
  {
    bool clear_bloom = true;
    remove_exact_match_filter_uint64(&filter_info,field,value);

    BLOOM_RESET_UINT64(category_filter_[cat].bitmap,&value);
    RWLOG_FILTER_DEBUG_PRINT("Added bit and bitmap is %lx\n", category_filter_[cat].bitmap);
    // New code
    hash_index = BLOOM_IS_SET(category_filter_[cat].bitmap,field_value_combo, is_set);
    is_set = is_set;
    filter_array_hdr *flt_ar_hdr = &rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
    int old_n_entries = flt_ar_hdr->n_entries;
    uint32_t new_hash_value_offset = l2_filter_remove(flt_ar_hdr,field_value_combo, clear_bloom);
    if(new_hash_value_offset)
    {
      flt_ar_hdr->table_offset = (new_hash_value_offset)&0xfffff;
      flt_ar_hdr->n_entries = (old_n_entries-1)&0xfff;
    }
    else {
      RW_DEBUG_ASSERT(flt_ar_hdr->n_entries == 1);
      flt_ar_hdr->table_offset = 0;
      flt_ar_hdr->n_entries = 0;
    }
    // Reference count
    if (clear_bloom)
    {
      BLOOM_RESET_UINT64(category_filter_[cat].bitmap,&value);
    }
  }
  if (action == FILTER_DENY_EVID)
  {
    RWLOG_SET_DENY_ID_HASH(rwlogd_filter_memory_->static_bitmap,value);
    // New code
    hash_index = BLOOM_IS_SET(category_filter_[0].bitmap,field_value_combo, is_set);
    is_set = is_set;
    filter_array_hdr *flt_ar_hdr = &rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
    int old_n_entries = flt_ar_hdr->n_entries;
    uint32_t new_hash_value_offset = l2_filter_add(flt_ar_hdr, field_value_combo);
    flt_ar_hdr->table_offset = (new_hash_value_offset)&0xfffff;
    flt_ar_hdr->n_entries = (old_n_entries+1)&0xfff;
    //
    RWLOG_FILTER_DEBUG_PRINT("Added bit and bitmap is %lx hash_idx = %lu offset =%d \n",
                             category_filter_[cat].bitmap, hash_index,
                             rwlogd_shm_ctrl_->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE].table_offset);
  }
  if(field_value_combo)
  {
    free(field_value_combo);
  }
}

bool rwlogd_sink_data::l2_exact_uint64_match(uint32_t cat, char *name, uint64_t value)
{
  filter_memory_header *mem_hdr = (filter_memory_header *) rwlogd_filter_memory_;
  uint64_t hash_index =  0;
  uint32_t pass = 0;
  char *field_value_combo = NULL;
  int r =  asprintf (&field_value_combo,
                     "%s:%lu:%d",
                     name,value,
                     cat);
  RW_ASSERT(r);
  if (!r) { return FALSE; }

  UNUSED(pass);
  hash_index = BLOOM_IS_SET(category_filter_[cat].bitmap,field_value_combo, pass);
  filter_array_hdr *fv_entry = &((rwlogd_shm_ctrl_t *)mem_hdr)->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
  if(fv_entry->table_offset)
  {
    int i=0;
    for (i=0; i<fv_entry->n_entries; i++)
    {
      char *str =((char *)mem_hdr + fv_entry->table_offset+i*RWLOG_FILTER_STRING_SIZE);
      if(strncmp(field_value_combo, str, RWLOG_FILTER_STRING_SIZE) == 0)
      {
        return(TRUE);
      }
    }
  }
  if (field_value_combo)
  {
    free (field_value_combo);
  }
  return(FALSE);
}

/*!
 * Fetch severity settings from shared memory 
 */
rwlog_severity_t rwlogd_sink_data::get_severity(uint32_t category)
{
  rwlog_severity_t severity;
  severity = (category >= num_categories_) ? 
             RW_LOG_LOG_SEVERITY_EMERGENCY:category_filter_[category].severity;
  return(severity);
}
/**************************************************************************
 * merge_severity .
 * Sets the Severity to lowest possible level 
 * ***********************************************************************/
rw_status_t rwlogd_sink_data::merge_severity(FILTER_ACTION action,
                                             uint32_t category,
                                             rwlog_severity_t severity)
{
  if (category < num_categories_) {
    incr_log_serial_no();

    if ((action == FILTER_ADD) && 
        (category_filter_[category].severity < severity)) {
      category_filter_[category].severity = severity;
    }
    rwlog_severity_t probable_severity = severity_[category]; //Start wirth Default

    for (auto &pe: *sink_queue_)  {
      if(pe->get_severity(category) > severity) {
        RWLOG_FILTER_DEBUG_PRINT("Sink Severity %d and desired %d Set %d\n", 
                                 pe->get_severity(category),severity, 
                                 rwlogd_filter_memory_->severity[category]);
        return RW_STATUS_SUCCESS;
      }
      if (probable_severity < pe->get_severity(category))
        probable_severity = pe->get_severity(category);
    }
    category_filter_[category].severity = probable_severity;
    RWLOG_FILTER_DEBUG_PRINT ("Filter Severity Set to  %d\n",
                              rwlogd_filter_memory_->severity[category]);
    return RW_STATUS_SUCCESS;
  }
  RWLOG_FILTER_DEBUG_PRINT ("Invalid Category  %d\n",category);

  return RW_STATUS_FAILURE;
}

void rwlogd_sink_data::merge_protocol_filter(uint32_t protocol)
{
  bool enable = FALSE;

  if(!(protocol< RW_LOG_PROTOCOL_TYPE_MAX_VALUE)) {
    return;
  }

  for (auto &pe: *sink_queue_)  {
    if(pe->is_protocol_filter_set(protocol) == TRUE) {
      enable = TRUE;
      break;
    }
  }

  RWLOG_FILTER_DEBUG_PRINT("Value before merging is %x-%x-%x-%x\n",
                           rwlogd_filter_memory_->protocol_filter[0], rwlogd_filter_memory_->protocol_filter[1], rwlogd_filter_memory_->protocol_filter[2], rwlogd_filter_memory_->protocol_filter[3]);

  if(enable) {
    RWLOG_FILTER_SET_PROTOCOL(rwlogd_filter_memory_,protocol);
  }
  else {
    RWLOG_FILTER_RESET_PROTOCOL(rwlogd_filter_memory_,protocol);
  }

  RWLOG_FILTER_DEBUG_PRINT("Value after merging for %d protocol: %u is %x-%x-%x-%x index: %lu bit: %lu\n",enable,protocol,
                           rwlogd_filter_memory_->protocol_filter[0], rwlogd_filter_memory_->protocol_filter[1], rwlogd_filter_memory_->protocol_filter[2], rwlogd_filter_memory_->protocol_filter[3],
                           (protocol/sizeof(rwlogd_filter_memory_->protocol_filter[0])), (protocol%sizeof(rwlogd_filter_memory_->protocol_filter[0])));

  RWLOG_UPDATE_PROTOCOL_FILTER_FLAG(rwlogd_filter_memory_,protocol_filter_set_);

  incr_log_serial_no();

}

void rwlogd_sink_data::merge_failed_call_filter() 
{
  bool shm_failed_call_enable = FALSE;

  for (auto &pe: *sink_queue_)  {
    if(pe->get_failed_call_filter() == TRUE) {
      shm_failed_call_enable = TRUE;
      break;
    }
  }

  update_failed_call_filter(shm_failed_call_enable);

}

void rwlogd_sink_data::merge_next_call_filter() 
{
  bool shm_next_call_enable = FALSE;

  for (auto &pe: *sink_queue_)  {
    if(pe->get_next_call_filter() == TRUE) {
      shm_next_call_enable = TRUE;
      break;
    }
  }

  update_next_call_filter(shm_next_call_enable);

}

int rwlogd_sink_data::map_category_string_to_index(char *category_str) {
  uint32_t i;
  for(i=0;i<num_categories_;i++) {
    if(!strncmp(category_list_[i],category_str,sizeof(category_str_t))) {
      return i;
    }
  }
  return -1;
}


/* Fn to get list of all categories. Currently this works by getting all
 * modules that have notifications in them.
 * */
void rwlogd_sink_data::get_category_name_list()
{
  const rw_yang_pb_schema_t *schema = yang_model_->get_ypbc_schema();
  RW_ASSERT(schema != NULL);
  if (!schema) { return; }
  num_categories_ = 0;

  /* Have category "all" as first category */
  strncpy(category_list_[num_categories_],"all",sizeof(category_str_t));
  num_categories_++;

  if(!yang_model_) {
    return;
  }

  /* Walk through list of modules in schema and if any have notifications
   * use module name as category name */
  for (size_t i = 0; i < schema->module_count; i++) {
    if (schema->modules[i]->notif_module && num_categories_ < RWLOG_MAX_NUM_CATEGORIES) {
      strncpy(category_list_[num_categories_],schema->modules[i]->module_name,sizeof(category_str_t));
      num_categories_++;
    }
  }

  if(num_categories_ > RWLOG_MAX_NUM_CATEGORIES) {
    fprintf(stderr,"Error: Exceeded max num of supported log categories %d\n",RWLOG_MAX_NUM_CATEGORIES);
  }
}


void rwlogd_sink_data::load_log_yang_modules(const char* schema_name)
{
  if (yang_model_) {
    return;
  }

  yang_model_ = rw_yang::YangModelNcx::create_model();
  RW_ASSERT(yang_model_);
  if (!yang_model_) { return; }

  char *env = getenv("YUMA_MODPATH");
  RWLOG_ASSERT(env);
  if (!env) {
    return;
  }
  rw_yang::YangModule *comp;

  // gtest 
  env = getenv("__RWLOG_CLI_SINK_GTEST_PRINT_LOG__");
  if(env)
  {
    comp = yang_model_->load_module("rw-logtest");
    RW_ASSERT(comp);
    if (!comp) { return; }
    comp->mark_imports_explicit();

    void *handle = dlopen ("liblog_test_yang_gen.so", RTLD_LAZY|RTLD_NODELETE);
    RW_ASSERT(handle);
    if (!handle) { return; }
    const auto *schema = (const rw_yang_pb_schema_t*)dlsym (handle, "rw_ypbc_RwLogtest_g_schema");
    RW_ASSERT(schema);
    yang_model_->register_ypbc_schema(schema);
  }
  else
  {
    comp = yang_model_->load_module(schema_name);
    RW_ASSERT(comp);
    if (!comp) { return; }
    comp->mark_imports_explicit();

    const auto *schema = rw_load_schema("librwlog-mgmt_yang_gen.so", schema_name);
    RW_ASSERT(schema);
    if (!schema) { return; }

    yang_model_->register_ypbc_schema(schema);
  }
}


void rwlogd_sink_data::dynamic_schema_update(rwsched_tasklet_t * tasklet_info,
                                                    rwsched_dispatch_queue_t queue,
                                                    const size_t batch_size,
                                                    rwdynschema_module_t *modules)
{
  if(!yang_model_) {
    return;
  }

  dynamic_queue_ = queue;
  tasklet_info_ = tasklet_info;
  dynamic_module_count_ = batch_size;
  dynamic_modules_ = modules;

  rwsched_dispatch_async_f(tasklet_info_,
                           dynamic_queue_,  
                           this,
                           dynamic_schema_load_modules);
}

void rwlogd_sink_data::dynamic_schema_load_modules(void * context)
{
  rwlogd_sink_data * instance = static_cast<rwlogd_sink_data *>(context);

  /* Get current schema and merge it with new schemas received */
  const rw_yang_pb_schema_t *schema = instance->yang_model_->get_ypbc_schema();
  for (size_t i = 0; i < instance->dynamic_module_count_; ++i) {
    //fprintf(stderr, "Module %lu is %s\n",i, (char *)instance->dynamic_modules_[i].module_name); 
    RW_ASSERT(instance->dynamic_modules_[i].module_name);
    if (!instance->dynamic_modules_[i].module_name) {
      instance->dynamic_status_ = RW_STATUS_FAILURE;
      break;
    }
    RW_ASSERT(instance->dynamic_modules_[i].so_filename);
    if (!instance->dynamic_modules_[i].so_filename) {
      instance->dynamic_status_ = RW_STATUS_FAILURE;
      break;
    }

    schema = rw_load_and_merge_schema(schema, instance->dynamic_modules_[i].so_filename, instance->dynamic_modules_[i].module_name);

    if (!schema) {
      instance->dynamic_status_ = RW_STATUS_FAILURE;
      break;
    }
  }

  if (instance->dynamic_status_ == RW_STATUS_FAILURE) {
    fprintf(stderr, "Failed to load dynamic schema \n");
    return;
  }

  /* Delete existing yang model and build it again with new schema */
  if(instance->yang_model_){
    delete instance->yang_model_;
  }
  instance->yang_model_ = NULL;

  instance->yang_model_ = rw_yang::YangModelNcx::create_model();
  RW_ASSERT(instance->yang_model_);
  if (!instance->yang_model_) {
    instance->dynamic_status_ = RW_STATUS_FAILURE;
    return;
  }

  /* Build new yang model */
  instance->yang_model_->load_schema_ypbc(schema);

  /* Annotate the yang model with the descriptors. */
  rw_status_t status = instance->yang_model_->register_ypbc_schema(schema);
  if ( RW_STATUS_SUCCESS != status ) {
    fprintf(stderr, "Failed to register ypbc schema \n");
  }

  rwsched_dispatch_async_f(instance->tasklet_info_,
                           rwsched_dispatch_get_main_queue(instance->tasklet_info_->instance),
                           instance,
                           dynamic_schema_end);


}

void rwlogd_sink_data::dynamic_schema_end(void * context)
{
  rwlogd_sink_data * instance = static_cast<rwlogd_sink_data *>(context);

   rw_status_t status = instance->update_category_list();
  if ( RW_STATUS_SUCCESS != status ) {
    fprintf(stderr, "Failed to update category list \n");
  }

  rwsched_dispatch_release(instance->tasklet_info_, instance->dynamic_queue_);
  if(instance->rwlogd_info_->rwlogd_instance && ((rwlogd_instance_ptr_t)instance->rwlogd_info_->rwlogd_instance)->dts_h) {
    rwdts_api_set_state((rwdts_api_t*)((rwlogd_instance_ptr_t)instance->rwlogd_info_->rwlogd_instance)->dts_h, RW_DTS_STATE_RUN);
  }

}

rw_status_t
rwlogd_sink_data::update_category_list()
{
  uint32_t current_num_categories = num_categories_;
  uint32_t i = 0;
  /* Get list of categoires from new model */
  get_category_name_list();

  /* If new log category is added; updated the shared memory */
  if(current_num_categories < num_categories_) { 
    if(num_categories_ >= RWLOG_MAX_NUM_CATEGORIES) {
      fprintf(stderr, "Max allowed Log categories exceeded; Ignoring new cateogires\n");
      return RW_STATUS_SUCCESS;
    }

    /* Update new categories */
    rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)rwlogd_filter_memory_ + sizeof(rwlogd_shm_ctrl_t)); 
    for(i=current_num_categories;i<num_categories_;i++) {
      memcpy((char *)category_filter[i].name,(char *)category_list_[i], sizeof(category_str_t));
      category_filter[i].severity = RW_LOG_LOG_SEVERITY_ERROR;
      category_filter[i].bitmap = 0;
      category_filter[i].critical_info = RW_LOG_ON_OFF_TYPE_ON;
      severity_[i] = RW_LOG_LOG_SEVERITY_ERROR;
    }
    rwlogd_filter_memory_->num_categories = num_categories_;

    /* Update new category in all the sinks */
    for (auto &pe: *sink_queue_) 
    { 
      pe->update_category_list(); 
    }
  }
  rwlogd_info_->num_categories = num_categories_;

  return RW_STATUS_SUCCESS;
}

void rwlogd_sink_data::gtest_fake_cache_entries()
{
  for (uint32_t i = 0; i< RWLOGS_MAX_CACHE_DESCRIPTORS*2; i++)
  {
    const ProtobufCMessageDescriptor *msg_desc = (const ProtobufCMessageDescriptor *)(1231);
    rwlog_msg_desc_key key1;
    key1.ns_id=i;
    key1.elem_id=rand();
    msg_descr_cache_->cache_add (key1, msg_desc);
  }
}
const ProtobufCMessageDescriptor *
rwlogd_sink_data::get_log_msg_desc(rw_schema_ns_and_tag_t schema_id)
{
  const ProtobufCMessageDescriptor *msg_desc = NULL;
  rw_yang::YangNode *evl_node = NULL;

  rwlog_msg_desc_key key1;
  key1.ns_id = schema_id.ns;
  key1.elem_id = schema_id.tag;

  msg_desc = msg_descr_cache_->cache_lookup(key1);
  if (!msg_desc) 
  {
    //Cache Miss 
    if (schema_id.ns == RW_NAMESPACE_ID_NULL) {
      evl_node = yang_model_->search_top_yang_node(schema_id.ns, schema_id.tag);
    } else {
      evl_node = yang_model_->search_node(schema_id.ns, schema_id.tag);
    }
    if (evl_node) {
      auto ypbc_msg = evl_node->get_ypbc_msgdesc();
      if (ypbc_msg) {
        msg_desc = ypbc_msg->pbc_mdesc;
      }
    }
    else {
      //RWLOG_DEBUG_PRINT("Msg Desc not found for log with ns_id: %d, element tag: %d\n",schema_id.ns,schema_id.tag);
      fprintf(stderr,"Msg Desc not found for log with ns_id: %d, element tag: %d\n",schema_id.ns,schema_id.tag);
      stats.msg_descr_not_found++;
    }
    RWLOG_DEBUG_PRINT("Cache Miss Adding %d \n", key1.ns_id);
    stats.msg_descr_cache_miss++;
    msg_descr_cache_->cache_add (key1, msg_desc);
  }
  else 
  {
    RWLOG_DEBUG_PRINT("Cache Hit Found %d %p \n", key1.ns_id, msg_desc);
    stats.msg_descr_cache_hit++;
  }
  return msg_desc;
}

rw_status_t rwlogd_sink_data::set_severity(uint32_t cat,
                                           rwlog_severity_t sev)
{
  uint32_t i = 0;
  if (cat < num_categories_)
  {
    if(cat == 0) {
      for(i=0;i<num_categories_;i++) {
        severity_[i] = sev;
        merge_severity(FILTER_ADD, i, sev);
      }
    }
    else {
      severity_[cat] = sev;
      merge_severity(FILTER_ADD, cat, sev);
    }
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;
}

void rwlogd_sink_data::update_failed_call_filter(bool enable)
{
  rwlogd_filter_memory_->failed_call_filter = enable;
}

void rwlogd_sink_data::update_next_call_filter(bool enable)
{
  rwlogd_filter_memory_->next_call_filter = enable;
}

void  rwlogd_sink::fill_packet_info(void *field_ptr,
                                    std::ostringstream &os,
                                    char *pdu_dump,
                                    char *pdu_hex_dump,
                                    int verbosity)
{         
  rw_pkt_info_t pkt_info;
  pkt_trace_t *tmp_pkt_info = (pkt_trace_t *)field_ptr;
  rwlogd_set_tcpdump_buf(verbosity);

  memset(&pkt_info, 0, sizeof(rw_pkt_info_t));

  if (tmp_pkt_info) {
    rwlog_update_pkt_info(&pkt_info, tmp_pkt_info); 
    rw_tcpdumplib_print((rwtcpdump_pkt_info_t *)&pkt_info, tmp_pkt_info->packet_data.data, tmp_pkt_info->packet_data.len, verbosity);
    if (pdu_dump) {
      std::ostringstream pkt_os;
      char *pkt_dump = rwlogd_get_tcpdump_buf();
      if(pkt_dump) {
        pkt_os << c_escape_string((const char *)pkt_dump, strlen(pkt_dump)).c_str();
        strncpy(pdu_dump,pkt_os.str().c_str(),MAX_PDU_SIZE);
      }
    } else {
      os << " packet-data" << (std::string)rwlogd_get_tcpdump_buf() << "\n";
    }
    if (verbosity == 5) {
      rwlogd_reset_tcpdump_buf(verbosity);
      hex_and_ascii_print("\n\t", (const u_char *)tmp_pkt_info->packet_data.data, tmp_pkt_info->packet_data.len);
      if (pdu_hex_dump) {
        std::ostringstream pkt_os;
        char *pkt_hex_dump = rwlogd_get_tcpdump_buf();
        if(pkt_hex_dump) {
          pkt_os << c_escape_string((const char *)pkt_hex_dump, strlen(pkt_hex_dump)).c_str();
          strncpy(pdu_hex_dump,pkt_os.str().c_str(),MAX_PDU_SIZE);
        }
      } else {
        os << " pdu-hex-dump" << (std::string)rwlogd_get_tcpdump_buf();
      }
    }
  }
  rwlogd_reset_tcpdump_buf(0);
  return;
}

/*****************************************************************
 * check_common_params 
 * The filter applied 
 * Common parameters have cid, sev and sess params
 * cid and sev are handled in the log_hdr
 * Only sess param here 
 ****************************************************************/
rwlog_status_t rwlogd_sink::check_common_params(const ProtobufCMessage *arrivingmsg,
                                                rwlog_hdr_t *log_hdr)

{
  common_params_t *common_params = (common_params_t *)((uint8_t *)arrivingmsg + log_hdr->common_params_offset);

  if(sink_filter_.check_severity((rwlog_severity_t)log_hdr->log_severity, 
                                 (uint32_t)log_hdr->log_category) == RWLOG_FILTER_MATCHED ||
     sink_filter_.check_critical_info_filter(log_hdr->critical_info,(uint32_t)log_hdr->log_category) == RWLOG_FILTER_MATCHED) {
    return RWLOG_FILTER_MATCHED;
  }

  if(common_params->has_call_identifier && common_params->call_identifier.has_callid) {
    char callid_field[] = "callid";
    if(sink_filter_.check_uint64_filter(callid_field, common_params->call_identifier.callid) == RWLOG_FILTER_MATCHED) {
      return RWLOG_FILTER_MATCHED;
    }
  }

  if(common_params->has_call_identifier && common_params->call_identifier.has_groupcallid) {
    char groupid_field[] = "groupcallid";
    if(sink_filter_.check_uint64_filter(groupid_field, common_params->call_identifier.groupcallid) == RWLOG_FILTER_MATCHED) {
      return RWLOG_FILTER_MATCHED;
    }
  }

  if(common_params->session_params) 
  {
    session_params_t *session_params = common_params->session_params;
    if(session_params->has_imsi) 
    {
      char imsi_field[] = "sess_param:imsi";
      if(sink_filter_.check_string_filter(log_hdr->log_category_str, log_hdr->log_category,
                                          imsi_field, session_params->imsi) == RWLOG_FILTER_MATCHED) 
      {
        return RWLOG_FILTER_MATCHED;
      }
    }
  }
  return RWLOG_CHECK_MORE_FILTERS;
}

rwlog_status_t rwlogd_sink::check_protocol_filter(const ProtobufCMessage *log,
                                                  rwlog_hdr_t *log_hdr,
                                                  rwlogd_filter &filter)
{
  const ProtobufCMessageDescriptor *log_desc = log->descriptor;
  const ProtobufCFieldDescriptor *fd = nullptr;

  fd = protobuf_c_message_descriptor_get_field_by_name(log_desc,RWLOG_BINARY_FIELD);
  if(fd)  {
    RWPB_T_MSG(RwLog_data_BinaryEvent_PktInfo) *pkt_info  = (RWPB_T_MSG(RwLog_data_BinaryEvent_PktInfo) *)((uint64_t)log + (fd->offset));
    if(filter.is_protocol_filter_set(pkt_info->packet_type) == TRUE && filter.is_packet_direction_match(pkt_info->packet_direction)) {
      return RWLOG_FILTER_MATCHED;
    }
  }
  return RWLOG_CHECK_MORE_FILTERS;
}

/*********************************************************************
 * Check Attributes 
 *
 ********************************************************************/
rwlog_status_t rwlogd_sink::check_filter_attributes(const ProtobufCMessage *log,
                                                    rwlog_hdr_t *log_hdr,
                                                    rwlogd_filter &filter) 
{
  const ProtobufCMessageDescriptor *log_desc = log->descriptor;
  const ProtobufCFieldDescriptor *fd = nullptr;
  void *field_ptr = nullptr;
  size_t off = 0;
  protobuf_c_boolean is_dptr = false;
  char *val = nullptr;

  RW_STATIC_ASSERT(sizeof(pid_t) == sizeof(uint32_t));
  if(filter.is_attribute_filter_set((uint32_t)log_hdr->log_category) == false)
  {
    return RWLOG_CHECK_MORE_FILTERS;
  }
  for (unsigned i = 0; i < log_desc->n_fields; i++) 
  {
    fd = &log_desc->fields[i];
 
    field_ptr = nullptr;
    protobuf_c_message_get_field_count_and_offset(log, fd, &field_ptr, &off, &is_dptr);

    switch (fd->type) {
      case  PROTOBUF_C_TYPE_STRING:
        val = (char *) field_ptr;
        if(filter.check_string_filter(log_hdr->log_category_str, log_hdr->log_category, (char *)fd->name,val) == RWLOG_FILTER_MATCHED)
        {
          return RWLOG_FILTER_MATCHED;
        }
        break;
      case PROTOBUF_C_TYPE_INT32:
        if(filter.check_int_filter(fd->name,*((int32_t *)field_ptr)) == RWLOG_FILTER_MATCHED) 
        {
          return RWLOG_FILTER_MATCHED;
        }
        break;

      case PROTOBUF_C_TYPE_UINT64:
        if(filter.check_uint64_filter(fd->name,*((uint64_t *)field_ptr)) == RWLOG_FILTER_MATCHED) 
        {
          return RWLOG_FILTER_MATCHED;
        }
        break;
      case PROTOBUF_C_TYPE_BYTES:
      case PROTOBUF_C_TYPE_MESSAGE:
        RWLOG_DEBUG_PRINT("PROTOBUF_C_TYPE_MESSAGE Not handled ****\n");
        break;
      default:
        RWLOG_DEBUG_PRINT("Default not handled ****\n");
        break;
    } // switch fd->type
  } //for loop
  return RWLOG_FILTER_DROP;
}
int rwlogd_sink::fill_log_string(ProtobufCMessage *arrivingmsg,
                                 rwlog_hdr_t *log_hdr,
                                 char *log_string,
                                 int log_string_size)
{
  int len=0;
  std::ostringstream os;
  fill_common_attributes(arrivingmsg, log_hdr, os);
  fill_attributes(arrivingmsg, os);
  len = os.str().length();
  strncpy(log_string, os.str().c_str(), log_string_size);
  if (len >= (int)log_string_size) 
  {
    len = log_string_size-1;
  }
  log_string[len] = '\0';
  return len;
}

severity_display_table_t severity_display[]= {
  {RW_LOG_LOG_SEVERITY_EMERGENCY,"emergency"},
  {RW_LOG_LOG_SEVERITY_ALERT,"alert"},
  {RW_LOG_LOG_SEVERITY_CRITICAL,"critical"},
  {RW_LOG_LOG_SEVERITY_ERROR, "error"},
  {RW_LOG_LOG_SEVERITY_WARNING,"warning"},
  {RW_LOG_LOG_SEVERITY_NOTICE, "notice"},
  {RW_LOG_LOG_SEVERITY_INFO, "info" },
  {RW_LOG_LOG_SEVERITY_DEBUG, "debug" }
};

int rwlogd_sink::fill_common_attributes(ProtobufCMessage *arrivingmsg,
                                        rwlog_hdr_t *log_hdr,
                                        std::ostringstream &os,
                                        rwlog_msg_id_key_t  *msg_id)
{
  common_params_t *common_params = (common_params_t *)((uint8_t *)arrivingmsg+log_hdr->common_params_offset);
  time_t evtime = common_params->sec;
  struct tm* tm = gmtime(&evtime);
  char tmstr[128];
  int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(tmstr+tmstroff, ".%06uZ", common_params->usec);
  os << tmstr;
  if (msg_id) {
    msg_id->tv_sec = common_params->sec;
    msg_id->tv_usec = common_params->usec;
    msg_id->pid = common_params->processid;
    msg_id->tid = common_params->threadid;
    msg_id->seqno = common_params->sequence;
    strncpy(msg_id->hostname, common_params->hostname, sizeof(msg_id->hostname));
  }
  os << "["<< common_params->hostname << "@" << common_params->appname << "-" << common_params->processid << "-" << common_params->threadid << "]";

  if (!common_params->version) {
    os << " version:" << common_params->version;
  } else {
    // packed major/minor, see rwlog.h
    RW_STATIC_ASSERT(RWLOG_SHIFT == 16);
    os << " version:" << (common_params->version >> 16) << "." << (common_params->version & 0x0000ffff);
  }
  std::string pathname = common_params->filename;
  os << " " << pathname.substr(pathname.find_last_of("/\\") + 1) << ":" << common_params->linenumber;

  // module.eventname
  os << " notification:";
  const ProtobufCMessageDescriptor *log_desc = arrivingmsg->descriptor;
  if (log_desc && log_desc->ypbc_mdesc && log_desc->ypbc_mdesc->module) {
    os << log_desc->ypbc_mdesc->module->module_name;
    os << "." << log_desc->ypbc_mdesc->yang_node_name;
  } else {
    os << "RIFTWARE ERROR: NULL LOG. PLEASE CHECK";
  }

  if(log_hdr->log_severity <= RW_LOG_LOG_SEVERITY_DEBUG) {
    os << " severity:" << severity_display[log_hdr->log_severity].string;
  }
  else {
    os << " severity:" << "invalid(" << log_hdr->log_severity <<")";
  }

  if(log_hdr->critical_info) {
    os << " critical-info";
  }

  // event id
  os << " event-id:" << common_params->event_id;   

  if(!uuid_is_null(log_hdr->vnf_id)) {
    char vnf_id_str[36+1];
    uuid_unparse(log_hdr->vnf_id,vnf_id_str);
    os << " vnf-id:" << vnf_id_str;
  }

  if(common_params->call_identifier.has_callid)
  {
    os << " callid:" << common_params->call_identifier.callid;
  }
  if(common_params->call_identifier.has_groupcallid)
  {
    os << " groupcallid:" << common_params->call_identifier.groupcallid;
  }
  return 0;
}

int rwlogd_sink::fill_attributes(const ProtobufCMessage *log,
                                 std::ostringstream &os,
                                 char *pdu_dump,
                                 char *pdu_hex_dump,
                                 int verbosity)
{
  /// loop for attributes
  const ProtobufCMessageDescriptor *log_desc = log->descriptor;
  const ProtobufCFieldDescriptor *fd = nullptr;
  void *field_ptr = nullptr;
  size_t off = 0;
  protobuf_c_boolean is_dptr = false;
  char s_val[RWLOGD_LOG_LINE_LEN];
  size_t s_len = sizeof(s_val);

  RW_STATIC_ASSERT(sizeof(pid_t) == sizeof(uint32_t));
  for (unsigned i = 0; i < log_desc->n_fields; i++)
  {
    fd = &log_desc->fields[i];

    /* If field name is vnf-id ignore displaying as log_hdr displays the same */
    if(!strcmp(fd->name,"vnf_id")) {
      continue;
    }

    field_ptr = nullptr;
    int count = protobuf_c_message_get_field_count_and_offset(log, fd, &field_ptr, &off, &is_dptr);
    if (count) 
    {
      s_len = sizeof(s_val);
      if (fd->type != PROTOBUF_C_TYPE_MESSAGE) {
        if (field_ptr)  {
          if (fd->type == PROTOBUF_C_TYPE_STRING) {
            os << " " << fd->name << ":";
            os << c_escape_string((const char *)field_ptr, strlen((char *)field_ptr)).c_str();
          } else {
            if (protobuf_c_field_get_text_value(NULL, fd, s_val, &s_len, field_ptr)) {
              os << " " <<  fd->name << ":";
              os << s_val;
            }
          }// String
        } // field_ptr
      } // Not a message
      else {
        if (field_ptr) {
          if (!strcmp(fd->name, "pkt_info"))
          {
            fill_packet_info(field_ptr, os, pdu_dump, pdu_hex_dump, verbosity);
          }
        }
        else
        {
          RWLOG_DEBUG_PRINT("Incorrect or No fields added for name %s\n",
                            fd->name);
        }
      }
    } //Count
  } //for loop
  return 0;
}
/////////
// get_unpacked_proto 
// Pending : cache as suggested by grant
////////
ProtobufCMessage *rwlogd_sink_data::get_unpacked_proto(uint8_t *buffer)
{
  rwlog_hdr_t *log_hdr= (rwlog_hdr_t *)buffer;
  RWLOG_DEBUG_PRINT("rwlogd_sink::unpack\n");
  RWLOG_ASSERT(log_hdr->magic==RWLOG_MAGIC);

  if (!log_hdr->schema_id.tag) {
    printf("get_unpacked_proto no elem_id*************\n");
    return NULL;
  }
  const ProtobufCMessageDescriptor *msg_desc = get_log_msg_desc(log_hdr->schema_id);

  ProtobufCMessage *arrivingmsg = nullptr;
  if (msg_desc) 
  {
    stats.unpack_proto++;
    arrivingmsg = protobuf_c_message_unpack(&protobuf_c_default_instance,
                                            msg_desc,
                                            log_hdr->size_of_proto,
                                            buffer+sizeof(rwlog_hdr_t));
  }
  else
  {
    char *env = getenv("__RWLOG_CLI_SINK_GTEST_PRINT_LOG__");
    if(!env)
    {
      printf("no Mesc_descriptor ns_id %d, log_hdr->elem_id %d\n", 
             log_hdr->schema_id.ns, log_hdr->schema_id.tag);

    }
    return NULL;
  }
  return arrivingmsg;
}

void rwlogd_sink_data::free_proto(ProtobufCMessage *proto)
{
  protobuf_c_message_free_unpacked(&protobuf_c_default_instance, proto);
}

rwlog_status_t rwlogd_sink::pre_process_evlog(uint8_t *proto)
{
  rwlog_hdr_t *log_hdr= (rwlog_hdr_t *)proto;
  rwlog_status_t filter_status;

  /* IF sink has VNF-ID configured, drop log if vnf-id doesnt match */
  if(!uuid_is_null(vnf_id_) && uuid_compare(vnf_id_,log_hdr->vnf_id)) {
    return RWLOG_FILTER_DROP;
  }

  filter_status = check_header_filter(proto);
  if(RWLOG_CHECK_MORE_FILTERS == filter_status)
  {
    if (sink_filter_.is_attribute_filter_set((uint32_t)log_hdr->log_category) == true)
    {
      return RWLOG_CHECK_MORE_FILTERS;
    }
    if(sink_filter_.is_protocol_filter_flag_set() == TRUE) {
      return RWLOG_CHECK_MORE_FILTERS;
    }
    // FIXME: ATTN this check has to be removed
    if(log_hdr->cid.callid || log_hdr->cid.groupid)
    {
      return RWLOG_CHECK_MORE_FILTERS;
    }
    return RWLOG_FILTER_DROP;
  }
  return RWLOG_FILTER_MATCHED;
}
int rwlogd_sink::apply_sink_filters(const ProtobufCMessage *arrivingmsg, rwlog_hdr_t *log_hdr)
{
  rwlog_status_t filter_status = check_common_params(arrivingmsg, log_hdr);
  if(RWLOG_FILTER_MATCHED == filter_status) {
    return RWLOG_FILTER_MATCHED;
  }
  else if(RWLOG_CHECK_MORE_FILTERS == filter_status)
  {
    filter_status = check_protocol_filter(arrivingmsg,log_hdr,sink_filter_);
    if(RWLOG_FILTER_MATCHED == filter_status) {
      return RWLOG_FILTER_MATCHED;
    }
    if(check_filter_attributes(arrivingmsg, log_hdr, sink_filter_) == RWLOG_FILTER_MATCHED)
    {
      // Done with all filters
      // If it is still RWLOG_CHECK_MORE_FILTERS . Drop the event.
      return RWLOG_FILTER_MATCHED;
    }
  }
  return RWLOG_FILTER_DROP;
}

/*****************************************************************
 * rwlogd_sink::convert_log_to_str 
 * Applies the filters configured on the sink
 * Fills the log string
 * @params
 *
 *****************************************************************/
int rwlogd_sink::convert_log_to_str(uint8_t *proto,
                                    char *string,
                                    size_t string_sz,
                                    bool ignore_filtering)
{
  ProtobufCMessage *arrivingmsg = rwlogd_sink_data::get_unpacked_proto(proto);
  if(!arrivingmsg)
  {
    return 0;
    //GTEST
  }
  int len = 0;
  if(ignore_filtering == TRUE || apply_sink_filters(arrivingmsg, (rwlog_hdr_t *)proto) != RWLOG_FILTER_DROP)
  {
    len = fill_log_string(arrivingmsg, (rwlog_hdr_t *)proto, string, string_sz);
  } 
  rwlogd_sink_data::free_proto(arrivingmsg);
  return len;
}

void  rwlogd_sink::add_session_callid_filter(uint8_t *proto, 
                                             rwlogd_filter &filter)
{
  rwlog_hdr_t *log_hdr= (rwlog_hdr_t *)proto;
  char callid_field[] = "callid";
  RWLOG_DEBUG_PRINT("rwlogd_sink::add_callid_filter\n");
  RWLOG_ASSERT(log_hdr->magic==RWLOG_MAGIC);

  if (!log_hdr->schema_id.tag) 
  {
    return;
  }

  if(log_hdr->cid_filter.cid_attrs.next_call_callid && get_next_call_filter() == TRUE) 
  {
    RWLOG_DEBUG_PRINT("Adding filter in sink for callid %lu for nextcall match\n",log_hdr->cid.callid);
    add_filter_uint64(callid_field,log_hdr->cid.callid);
    //if(log_hdr->next_call_callid) {
    /* Rest filter if this is a next call callid match */
    update_next_call_filter(FALSE);
    //}
  }
  else if(log_hdr->cid_filter.cid_attrs.failed_call_callid && get_failed_call_filter() == TRUE) {
    RWLOG_DEBUG_PRINT("Adding filter in sink for callid %lu for next failed call match\n",log_hdr->cid.callid);
    add_filter_uint64(callid_field,log_hdr->cid.callid);
    if(get_next_failed_call_filter() == TRUE) {
      /* Rest filter if this is a next call callid match */
      update_failed_call_filter(FALSE,FALSE);
      /* TODO - Start a timer to cleanup callid filter */
    }
  }
  else {
    const ProtobufCMessageDescriptor *msg_desc = rwlogd_sink_data::get_log_msg_desc(log_hdr->schema_id);

    if (msg_desc) {
      ProtobufCMessage *arrivingmsg = protobuf_c_message_unpack(&protobuf_c_default_instance,
                                                                msg_desc,
                                                                log_hdr->size_of_proto,
                                                                proto+sizeof(rwlog_hdr_t));

      if(log_hdr->cid_filter.cid_attrs.add_callid_filter && arrivingmsg) {
        const ProtobufCFieldDescriptor *fd = nullptr;
        uint32_t common_params_offset = log_hdr->common_params_offset;
        if (!common_params_offset)
        {
          fd = protobuf_c_message_descriptor_get_field_by_name(msg_desc,"template_params");
          common_params_offset = fd->offset;
        }
        if (common_params_offset) 
        {
          common_params_t *common_params = nullptr;
          common_params = (common_params_t *)((uint8_t *)arrivingmsg + common_params_offset);
          if(common_params->session_params) {
            session_params_t *session_params = common_params->session_params;
            if(session_params->has_imsi) {
              char imsi_field[] = "sess_param:imsi";
              char cat_str[] = "all";
              filter_attribute *attr = find_exact_match_filter_string(&filter.filter_info,cat_str,imsi_field,session_params->imsi);
              if (attr) {
                /* Add generate callid filter in this sink */
                RWLOG_DEBUG_PRINT("Adding filter in sink for callid %lu\n",log_hdr->cid.callid);
                add_filter_uint64(callid_field,log_hdr->cid.callid);

                /* session-params next-call filter */
                if(attr->next_call_flag == 1) { 
                  uint32_t cat = 0;
                  rw_status_t  status = filter_operation(FILTER_DEL,data_container_->category_list_[cat], imsi_field,session_params->imsi,-1);
                  if(status == RW_STATUS_SUCCESS) {
                    data_container_->merge_filter(FILTER_DEL, data_container_->category_list_[cat],imsi_field,session_params->imsi);
                  } 
                } /* End of next call flag */
              }
            }
          }
        }
      }
    }
  }
}

void  rwlogd_sink::remove_session_callid_filter(uint64_t callid, 
                                                rwlogd_filter &filter)
{
  char callid_field[] = "callid";
  remove_filter_uint64(callid_field,callid);
}

rwlogd_sink:: rwlogd_sink ()
{
  log_filter_matched = 0;
  uuid_clear(vnf_id_);
  RWLOG_FILTER_DEBUG_PRINT("sink %s: created******\n", sink_name_);
}
rwlogd_sink:: ~rwlogd_sink()
{
  RWLOG_FILTER_DEBUG_PRINT("sink %s: Destroyed******\n", sink_name_);
  for(uint32_t cat =0 ; cat<data_container_->num_categories_; cat++)
  {
    data_container_->merge_severity(FILTER_DEL,
                                    cat,
                                    RW_LOG_LOG_SEVERITY_ERROR);
  }
  if (data_container_->sink_queue_)
    data_container_->sink_queue_->remove(this);
}

rwlog_severity_t rwlogd_sink::get_severity(uint32_t category)
{
  return sink_filter_.get_severity(category);
}


rw_status_t rwlogd_sink::set_severity(uint32_t cat,
                                      rwlog_severity_t sev)
{
  if (cat < data_container_->num_categories_)
  {
    if(cat) {
      sink_filter_.set_severity(cat,sev);
    }
    else {
      for(uint32_t index = 0;index < data_container_->num_categories_; index++) {
        sink_filter_.set_severity(index,sev);
      }
    }
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;
}

void rwlogd_sink::update_failed_call_filter(bool enable,bool next_call_filter)
{
  sink_filter_.update_failed_call_filter(enable,next_call_filter);
}

bool rwlogd_sink::get_next_failed_call_filter() 
{
  return sink_filter_.get_next_failed_call_filter();
}

bool rwlogd_sink::get_failed_call_filter() 
{
  return sink_filter_.get_failed_call_filter();
}

void rwlogd_sink::update_next_call_filter(bool enable)
{
  sink_filter_.update_next_call_filter(enable);
}

bool rwlogd_sink::get_next_call_filter() 
{
  return sink_filter_.get_next_call_filter();
}

int rwlogd_sink::handle_evlog(uint8_t *buffer)
{
  if(pre_process_evlog(buffer) != RWLOG_FILTER_DROP)
  {
    return post_evlog(buffer);
  }
  return 0;
}
rwlog_status_t rwlogd_sink::check_header_filter(uint8_t *proto)
{
  rwlog_hdr_t *hdr= (rwlog_hdr_t *)proto;
  if(sink_filter_.check_severity((rwlog_severity_t)hdr->log_severity, (uint32_t)hdr->log_category) == RWLOG_FILTER_MATCHED ||
     sink_filter_.check_critical_info_filter(hdr->critical_info,(uint32_t)hdr->log_category) == RWLOG_FILTER_MATCHED) {
    return RWLOG_FILTER_MATCHED;
  }
  /*
    if(filter_.callid_check(hdr->cid))
    {
    return RWLOG_FILTER_MATCHED;
    }
  */
  return RWLOG_CHECK_MORE_FILTERS;
}
int rwlogd_sink::post_evlog(uint8_t *proto)
{
    RW_CRASH();
  return -1;
}
void rwlogd_sink::set_backptr(rwlogd_sink_data *ptr)
{
  data_container_ = ptr;
}

void rwlogd_sink::create_category_list()
{
  sink_filter_.category_filter_ = (rwlog_category_filter_t *)RW_MALLOC0(RWLOG_MAX_NUM_CATEGORIES * sizeof(rwlog_category_filter_t));
  RW_ASSERT(data_container_->num_categories_ < RWLOG_MAX_NUM_CATEGORIES);
  if (data_container_->num_categories_ >= RWLOG_MAX_NUM_CATEGORIES) {
    data_container_->num_categories_ = RWLOG_MAX_NUM_CATEGORIES-1;
  }
  RW_ASSERT(sink_filter_.category_filter_);
  if (!sink_filter_.category_filter_) {
    return;
  }
  for(uint32_t i=0;i<data_container_->num_categories_;i++) {
    sink_filter_.category_filter_[i].severity = RW_LOG_LOG_SEVERITY_ERROR;
    sink_filter_.category_filter_[i].bitmap = 0;
    sink_filter_.category_filter_[i].critical_info = RW_LOG_ON_OFF_TYPE_ON;
  }
  sink_filter_.filter_hdr_.num_categories = data_container_->num_categories_;
}

void rwlogd_sink::update_category_list()
{
  RW_ASSERT(data_container_->num_categories_ < RWLOG_MAX_NUM_CATEGORIES);
  if(data_container_->num_categories_ >= RWLOG_MAX_NUM_CATEGORIES) {
     data_container_->num_categories_ = RWLOG_MAX_NUM_CATEGORIES-1;
  }
  for(uint32_t i=sink_filter_.filter_hdr_.num_categories;i<data_container_->num_categories_;i++) {
    sink_filter_.category_filter_[i].severity = RW_LOG_LOG_SEVERITY_ERROR;
    sink_filter_.category_filter_[i].bitmap = 0;
    sink_filter_.category_filter_[i].critical_info = RW_LOG_ON_OFF_TYPE_ON;
  }
  sink_filter_.filter_hdr_.num_categories = data_container_->num_categories_;
}

rw_status_t rwlogd_sink::add_filter(char *category_str,
                                    char* attr, char *value,
                                    int next_call_flag)
{
  RWLOG_FILTER_DEBUG_PRINT("sink %s: Adding Filter ******\n", sink_name_);
  add_exact_match_filter_string(&sink_filter_.filter_info,category_str,attr,value,next_call_flag);
  int cat = data_container_->map_category_string_to_index(category_str);
  BLOOM_SET(sink_filter_.category_filter_[cat].bitmap,value);
  return RW_STATUS_SUCCESS;
}
rw_status_t rwlogd_sink::remove_filter(char *category_str, 
                                       char *attr, char *value)
{
  RWLOG_FILTER_DEBUG_PRINT("sink %s: Removing Filter ******\n", sink_name_);
  remove_exact_match_filter_string(&sink_filter_.filter_info,category_str, attr,value);
  return RW_STATUS_SUCCESS;
}

rw_status_t rwlogd_sink::deny_evid_filter(char* attr, char *value)
{
  RWLOG_FILTER_DEBUG_PRINT("sink %s: Adding deny Filter ******\n", sink_name_);
  add_exact_match_deny_filter_string(&sink_filter_.deny_filter_info,attr,value);
  return RW_STATUS_SUCCESS;
}
rw_status_t rwlogd_sink::add_filter_uint64(char* attr, uint64_t value)
{
  RWLOG_FILTER_DEBUG_PRINT("sink %s: Adding Filter ******\n", sink_name_);
  add_exact_match_filter_uint64(&sink_filter_.filter_info,attr,value);
  //BLOOM_SET(sink_filter_.filter_hdr_.bitmap[cat],value);
  return RW_STATUS_SUCCESS;
}
bool rwlogd_sink::get_filter_uint64(char* attr, uint64_t value)
{
  RWLOG_FILTER_DEBUG_PRINT("sink %s: Adding Filter ******\n", sink_name_);
  return(RWLOG_FILTER_MATCHED ==find_exact_match_filter_uint64(&sink_filter_.filter_info,attr,value));
}

rw_status_t rwlogd_sink::remove_filter_uint64(char* attr, uint64_t value)
{
  RWLOG_FILTER_DEBUG_PRINT("sink %s: Removing Filter ******\n", sink_name_);
  remove_exact_match_filter_uint64(&sink_filter_.filter_info,attr,value);
  return RW_STATUS_SUCCESS;
}

rw_status_t rwlogd_sink::filter_operation(FILTER_ACTION act,
                                          char *category_str,
                                          char *attr, char *value,
                                          int next_call_flag)
{
  switch (act) {
    case FILTER_ADD:
      return add_filter(category_str, attr,value,next_call_flag);
      break;
    case FILTER_DENY_EVID:
      return deny_evid_filter(attr,value);
      break;
    case FILTER_DEL:
      return remove_filter(category_str, attr,value);
      break;
    default:
      break;
  }
  return RW_STATUS_FAILURE;
}

rw_status_t rwlogd_sink::filter_uint64(FILTER_ACTION act,
                                       char *attr,
                                       uint64_t value)
{
  return add_filter_uint64(attr,value);
}
void rwlogd_sink::rwlog_dump_info(int verbose)
{
  printf ("Name = %s \n", sink_name_);
  printf ("state = %d \n", _state);
  if(!uuid_is_null(vnf_id_)) {
    char vnf_id_str[36+1];
    uuid_unparse(vnf_id_,vnf_id_str);
    printf("vnf-id = %s \n", vnf_id_str);
  }

  printf ("Matched log count = %lu \n", log_filter_matched);
  sink_filter_.rwlog_dump_info(verbose);
}


void *rwlogd_create_show_log_filter(rwlogd_instance_ptr_t inst,rwlog_severity_t default_severity,
                                    RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *filter_cfg)
{
  uint32_t n_attributes;
  uint32_t index,i;
  rw_status_t status =  RW_STATUS_FAILURE;
  int filter_attr_set = 0;
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)  *category = NULL; 
  int category_index = RW_LOG_LOG_CATEGORY_ALL;

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_filter *filter= new rwlogd_filter();

  filter->filter_hdr_.num_categories = obj->num_categories_;

  filter->category_filter_ = (rwlog_category_filter_t *)RW_MALLOC0(filter->filter_hdr_.num_categories * sizeof(rwlog_category_filter_t));
  RW_ASSERT(filter->category_filter_);
  if (!filter->category_filter_) {
    return NULL;
  }
  for (index=0; index<filter->filter_hdr_.num_categories ;index++) {
    filter->category_filter_[index].severity = default_severity;
    filter->category_filter_[index].bitmap = 0;
  }

  if(!filter_cfg) {
    filter->num_filters = 1;
    return filter;
  }

  if ( filter_cfg->n_category ) {
    category = filter_cfg->category[0];
    category_index = obj->map_category_string_to_index(category->name);
    if(category_index < 0) {
      return filter;
    }
  }

  if(filter_cfg->binary_events) {
    if(filter_cfg->binary_events->has_protocol) {
      filter->protocol_filter_set_ = TRUE;
      RWLOG_FILTER_SET_PROTOCOL(&filter->filter_hdr_,filter_cfg->binary_events->protocol);
    }
    if (filter_cfg->binary_events->has_packet_direction) {
      RWLOG_FILTER_SET_PACKET_DIRECTION(&filter->filter_hdr_,filter_cfg->binary_events->packet_direction);
    }
  }

  if (filter_cfg->n_category == 0 && 
      filter_cfg->has_callid == 0 && 
      filter_cfg->has_groupcallid == 0) {
    //filter_attr_set=1; //Use all-all filter
    //return RW_STATUS_SUCCESS;
    return filter;
  }

  if (category && category->n_attribute) {
    n_attributes = category->n_attribute;
    for (index = 0; index < n_attributes; index++) {
      if (category_index != RW_LOG_LOG_CATEGORY_ALL) {
        BLOOM_SET(filter->category_filter_[category_index].bitmap,category->attribute[index]->value);
        filter->category_filter_[category_index].severity = RW_LOG_LOG_SEVERITY_DEBUG;
        status  =  add_exact_match_filter_string(&filter->filter_info,
                                                 category->name,
                                                 category->attribute[index]->name, 
                                                 category->attribute[index]->value,-1) ;
      } else {
        category_str_t *category_list = obj->category_list_;
        for (i=1; i<filter->filter_hdr_.num_categories;i++) {
          BLOOM_SET(filter->category_filter_[i].bitmap,category->attribute[index]->value);
          filter->category_filter_[i].severity = RW_LOG_LOG_SEVERITY_DEBUG;
          status  =  add_exact_match_filter_string(&filter->filter_info,
                                                   category_list[i],
                                                   category->attribute[index]->name,
                                                   category->attribute[index]->value,-1) ;
        }
      }
      if(status == RW_STATUS_SUCCESS) {
        filter_attr_set+=2; // Sev + Attr
      }
    }
  }
  if (filter_cfg->has_callid) {
    char callid_str[] = "callid";
    add_exact_match_filter_uint64(&filter->filter_info, callid_str,filter_cfg->callid);
    filter_attr_set++;
  }

  if (filter_cfg->has_groupcallid) {
    char groupid_str[] = "groupcallid";
    add_exact_match_filter_uint64(&filter->filter_info, groupid_str,filter_cfg->groupcallid);
    filter_attr_set++;
  }

#if 0
  if (category && category->packet) {
    rwlog_show_log_apply_packet_filter(instance, category->packet);
    filter_attr_set++;
  }
#endif

  
  if ((category) && 
      ((category->has_severity) || (filter_attr_set == 0))) {
    //No attr set is like setting severity filter....

    rwlog_severity_t severity = default_severity;
    if (category->has_severity) {
      severity = category->severity;
    }
    if (category_index == RW_LOG_LOG_CATEGORY_ALL) {
      for (index=0; index<filter->filter_hdr_.num_categories;index++) {
        filter->category_filter_[index].severity = severity;
      }
    }
    else {
      filter->category_filter_[category_index].severity = severity;
    }
    filter_attr_set++;
  }
  else if(filter_cfg->has_callid || filter_cfg->has_groupcallid) {
    for (i=0; i<filter->filter_hdr_.num_categories ;i++) {
      filter->category_filter_[i].severity = RW_LOG_LOG_SEVERITY_DEBUG;
    }
    filter_attr_set++;  /* Since severity filter is also set update it */
  }

  // Do not add filter checks after this severity block.
  filter->num_filters = filter_attr_set;
  return filter;
}

rw_status_t rwlogd_delete_show_log_filter(rwlogd_instance_ptr_t inst,
                                          void *show_filter)
{
  delete  (rwlogd_filter *)show_filter;
  return RW_STATUS_SUCCESS;
}
