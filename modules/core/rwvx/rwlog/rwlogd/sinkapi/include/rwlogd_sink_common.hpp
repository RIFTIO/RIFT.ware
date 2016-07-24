
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __RWLOGD_SINK_COMMON_H__
#define __RWLOGD_SINK_COMMON_H__

#include <list>
#include "rwlogd_common.h"
#include "rw-log.pb-c.h"
#include "yangncx.hpp"
#include "rwlogd_utils.h"
#include "rwlogd_filters.hpp"
#include "rwdynschema.h"
/*******************************************************************************
        Class: rwlogd_sink_data 
        ========================
        Exists as void* in instance data
        Maintains All the Sink queues. 
        Maintains All Eventlog circular buffer for the queus
        Maintains the filter shared memory.
        Maintains the Yang data. 
******************************************************************************/
#define SINKNAMESZ 32
#define RWLOG_PRINT_SZ 1024
#define RWLOG_TIME_STRSZ 128
#define RWLOGS_MAX_CACHE_DESCRIPTORS 128
#define BOOTSTRAP_SYSLOG_SINK "localhost-syslog"
enum connection_state {
  INIT,
  RESOLVED,
  CONNECTED,
  BAD,
};
enum rwlogd_shm_bkt_sz {
  RWLOGD_SHM_FILTER_SIZE_8,
  RWLOGD_SHM_FILTER_SIZE_256,
  RWLOGD_SHM_ARRAY_BOUNDARY
};


typedef struct  {
  rwlog_severity_t severity;
  const char *string;           
}severity_display_table_t;


class rwlog_msg_desc_key {
 public:
  uint32_t ns_id;
  uint32_t elem_id;
  bool operator ==(const rwlog_msg_desc_key &key1) const
  {
    if ((ns_id==key1.ns_id) &&
        (elem_id==key1.elem_id))
    {
      return true;
    }
    return false;
  }
  bool operator <(const rwlog_msg_desc_key &key1) const
  {
    if (ns_id==key1.ns_id)
    {
      if (elem_id<key1.elem_id)
      {
        return true;
      }
    }
    if (ns_id<key1.ns_id)
    {
      return true;
    }
    return false;
  }
};

typedef RWPB_T_MSG(RwLog_data_EvTemplate_TemplateParams) common_params_t;
typedef RWPB_T_MSG(RwLog_data_BinaryEvent_PktInfo) pkt_trace_t;
typedef rw_cache<rwlog_msg_desc_key , const ProtobufCMessageDescriptor *> msg_descr_cache_t;
class rwlogd_sink;
class rwlogd_filter;
class rwlog_free_mem_hdr_t {
 public:
  int size;
};
class rwlogd_sink_data{
  //TBD: Singleton 
 public:
  rwlogd_sink_data(rwlogd_tasklet_instance_ptr_t rwlogd_info, char *filter_shm_name, const char* schema_name);
  ~rwlogd_sink_data();

  rwlogd_sink_data(const rwlogd_sink_data& other) = delete;
  void operator=(const rwlogd_sink_data& other) = delete;
 public:
  typedef std::list<rwlogd_sink*> sink_queue_t; // Sinks
  std::list<uint32_t> shm_free_list;

  sink_queue_t *sink_queue_;
  static msg_descr_cache_t *msg_descr_cache_;
  static uint32_t num_categories_;
  static category_str_t *category_list_;
  rwlogd_sink *default_sink;

  // Sink  APIs
  rw_status_t add_sink(rwlogd_sink *snk);
  rwlogd_sink *remove_sink(const char *name);
  rwlogd_sink *lookup_sink(const char *name);
 
  // Log Ring APIs
  rw_status_t add_log_to_ring(uint8_t *buffer);
  rw_status_t get_log_timestamp(int pos, struct timeval *tv, char *buf, size_t buf_sz);
  rw_status_t get_log_trailing_timestamp(int pos, char *buf, size_t buf_sz);
  rw_status_t handle_evlog(uint8_t *buffer);

  // Filter APIs
  rw_status_t set_severity(uint32_t cat, rwlog_severity_t sev);

  void incr_log_serial_no() {rwlogd_filter_memory_->log_serial_no ++; };
  void incr_log_serial_no(int cnt) {rwlogd_filter_memory_->log_serial_no +=cnt; };

  void update_next_call_filter(bool next_call_filter);
  void merge_next_call_filter();

  void update_failed_call_filter(bool next_call_filter);
  void merge_failed_call_filter();


  void rwlogd_filter_init(char *filter_shm_name);
  void merge_filter(FILTER_ACTION act, char *category_str,
                    char *field, char *value);
  uint32_t l2_filter_add(filter_array_hdr *flt_hdr,
                         char *value);
  uint32_t l2_filter_remove(filter_array_hdr *flt_hdr,
                            char *value,
                            bool &clear_bloom);
  bool l2_exact_uint64_match(uint32_t cat, char *name, uint64_t value);
  void merge_filter_uint64(FILTER_ACTION, 
                           uint32_t category,
                           char *, uint64_t);
  void set_dup_events_flag(bool flag);
  rw_status_t merge_severity(FILTER_ACTION act, uint32_t category,
                             rwlog_severity_t severity);
  rwlog_severity_t get_severity(uint32_t category);

  void merge_protocol_filter(uint32_t protocol);
  bool is_protocol_filter_set(uint32_t protocol) { return RWLOG_FILTER_IS_PROTOCOL_SET(rwlogd_filter_memory_,protocol); }

  void handle_file_rotation();
  uint32_t shm_buffer_alloc(int request_size, rwlogd_shm_bkt_sz bucket);
  void shm_buffer_dump_free_blk();
  void shm_buffer_free(uint32_t block_offset, int freed_size, rwlogd_shm_bkt_sz buccket);
  void shm_reset_bootstrap();
  void enable_L1_filter();
  void shm_incr_ticks() { rwlogd_filter_memory_->rwlogd_ticks++; }
  void shm_set_flow() { rwlogd_filter_memory_->rwlogd_flow = 1; }
  void shm_reset_flow() { rwlogd_filter_memory_->rwlogd_flow = 0; }
  static ProtobufCMessage *get_unpacked_proto(uint8_t *buffer);
  static void free_proto(ProtobufCMessage *proto);

 
  // Yang model 
  static void load_log_yang_modules(const char* schema_name);
  void dynamic_schema_update(rwsched_tasklet_t * tasklet_info,                                  
                                    rwsched_dispatch_queue_t queue,
                                    const size_t batch_size,
                                    rwdynschema_module_t *modules);

  static void dynamic_schema_load_modules(void * context);
  static void dynamic_schema_end(void * context);

  static void get_category_name_list();
  rw_status_t update_category_list();
  static int map_category_string_to_index(char *category_str);

  static rw_yang::YangModel* get_yang_model() { return yang_model_; }

  static const ProtobufCMessageDescriptor *
  get_log_msg_desc(rw_schema_ns_and_tag_t schema_id);

 private:
  rw_status_t dynamic_status_ = RW_STATUS_SUCCESS;
  rwsched_dispatch_queue_t dynamic_queue_ = nullptr;
  rwsched_tasklet_t * tasklet_info_ = nullptr;
  rwlogd_tasklet_instance_ptr_t rwlogd_info_ = nullptr;
  uint64_t dynamic_module_count_ = 0;
  rwdynschema_module_t *dynamic_modules_ = nullptr;
  static rw_yang::YangModel *yang_model_;
  int filter_shm_fd_;
  char *rwlog_shm_name_;
  rwlogd_shm_ctrl_t *rwlogd_shm_ctrl_; // From SHM
  filter_memory_header *rwlogd_filter_memory_; // From SHM
  filter_attribute *filter_info; //  Used for local filtering Sinks and SinkOBJ
  filter_attribute *deny_filter_info; // Used for deny event-ID filtering for Sinks
  //rwlog_severity_t severity_[RW_LOG_LOG_CATEGORY_MAX_VALUE]; // Severity table
  rwlog_severity_t *severity_;
  rwlog_category_filter_t *category_filter_;

  bool protocol_filter_set_;
  struct sink_data_stats {
    uint64_t msg_descr_cache_miss;
    uint64_t msg_descr_not_found;
    uint64_t msg_descr_cache_hit;
    uint64_t unpack_proto;
    void rwlog_dump_info(int verbose)
    {
      printf("Statistics:\n");
      printf("\tmsg_descr_not_found %lu\n", msg_descr_not_found);
      printf("\tmsg_descr_cache_miss %lu\n", msg_descr_cache_miss);
      printf("\tmsg_descr_cache_hit %lu\n", msg_descr_cache_hit);
      printf("\tunpack_proto %lu\n", unpack_proto);
    }
  };
  static sink_data_stats stats;
 public:
  void rwlog_dump_info(int verbose);
  void gtest_fake_cache_entries();
};

/*********************************************
 *
 * RwLogSink Base Class 
 *
 ********************************************/
class rwlogd_sink {
  connection_state _state;
 public:
  inline void set_state(connection_state state)
  {
    _state  = state;
  }
  inline connection_state get_state()
  {
    return _state;
  }
  char sink_name_[SINKNAMESZ];
  rwlogd_filter sink_filter_;
  uint64_t  log_filter_matched;
  uuid_t vnf_id_;

  rwlogd_sink ();
  virtual ~rwlogd_sink();
  virtual void rwlog_dump_info(int verbose);
  const char *get_name() { return sink_name_; }

  rwlog_severity_t get_severity(uint32_t category);
  rw_status_t set_severity(uint32_t cat, 
                           rwlog_severity_t sev);

  void set_protocol_filter(uint32_t protocol, bool enable) { sink_filter_.set_protocol_filter(protocol,enable); }
  bool is_protocol_filter_set(uint32_t protocol) { return sink_filter_.is_protocol_filter_set(protocol); }

  void update_vnf_id(char *vnf_id) { uuid_parse(vnf_id,vnf_id_); }

  uint64_t matched_log_count() { return log_filter_matched;}

  bool get_next_call_filter();
  void update_next_call_filter(bool next_call_filter);

  bool get_next_failed_call_filter();
  bool get_failed_call_filter();
  void update_failed_call_filter(bool failed_call_filter,bool next_call_filter);

  virtual int post_evlog(uint8_t *proto);
  virtual void  remove_log_from_ring(uint8_t *buffer) { return; }
  virtual void  clear_log_from_ring() { return; }
  virtual void get_log_ring_stats(ring_buffer_stats_t *stats) { return ;}
  virtual void add_generated_callid_filter(uint8_t *proto) { add_session_callid_filter(proto,sink_filter_); }
  virtual void remove_generated_callid_filter(uint64_t callid) { remove_session_callid_filter(callid,sink_filter_);}
  virtual rw_status_t get_log(int &pos, rwlogd_output_specifier_t *os) {return RW_STATUS_SUCCESS;};
  virtual rw_status_t get_position(int &pos, struct timeval *start_time) {return RW_STATUS_SUCCESS;};
  virtual int get_last_position() {return 0;};
  virtual rw_status_t get_log_trailing_timestamp(int pos, char *buf, size_t buf_sz) 
  {return RW_STATUS_SUCCESS;}
  virtual void handle_file_log(char *log_string) {}
  rwlog_status_t check_header_filter(uint8_t *proto);
  rwlog_status_t check_common_params(const ProtobufCMessage *arrivingmsg,
                                     rwlog_hdr_t *log_hdr);
  rwlog_status_t check_filter_attributes(const ProtobufCMessage *log,
                                         rwlog_hdr_t *log_hdr,
                                         rwlogd_filter &);
  rwlog_status_t check_protocol_filter(const ProtobufCMessage *log,
                                       rwlog_hdr_t *log_hdr,
                                       rwlogd_filter &);
  virtual rwlog_status_t pre_process_evlog(uint8_t *proto);
  int apply_sink_filters(const ProtobufCMessage *arrivingmsg, rwlog_hdr_t *log_hdr);
  int fill_log_string(ProtobufCMessage *arrivingmsg,
                      rwlog_hdr_t *log_hdr,
                      char *log_string,
                      int log_string_size);
  virtual int fill_common_attributes(ProtobufCMessage *arrivingmsg, 
                                     rwlog_hdr_t *log_hdr,
                                     std::ostringstream &os,
                                     rwlog_msg_id_key_t  *msg_id=0);
  int fill_attributes(const ProtobufCMessage *, 
                      std::ostringstream &os,
                      char *pdu_dump=0,
                      char *pdu_hex_dump=0, 
                      int verbosity=0);
  virtual int handle_evlog(uint8_t *buffer);


  int convert_log_to_str(uint8_t *proto, char *string, size_t str_sz,bool ignore_filtering=FALSE);

  void fill_packet_info(void *field_ptr,
                        std::ostringstream &os,
                        char *pdu_dump,
                        char *pdu_hex_dump,
                        int verbosity);

  void add_session_callid_filter(uint8_t *proto, 
                                 rwlogd_filter &filter);
  void remove_session_callid_filter(uint64_t callid, 
                                    rwlogd_filter &filter);

  void set_backptr(rwlogd_sink_data *ptr);
  void create_category_list();
  void update_category_list();
  rw_status_t add_filter(char *category_str ,
                         char* attr, char *value,
                         int next_call_flag) ;
  rw_status_t remove_filter(char *category_str,
                            char *att, char *value);
  rw_status_t deny_evid_filter(char* attr, char *value) ;
  rw_status_t add_filter_uint64(char* attr, uint64_t value); 
  bool get_filter_uint64(char* attr, uint64_t value); 
  rw_status_t remove_filter_uint64(char* attr, uint64_t value); 
  rw_status_t filter_operation(FILTER_ACTION act, char *category_str,
                               char *attr, char *value,
                               int next_call_flag);
  rw_status_t filter_uint64(FILTER_ACTION act, char *attr, uint64_t value);

 private:
  rwlogd_sink_data *data_container_;
};

//rwlogd_sink
/********************************************
 * RT -realtime sinks
 * calls post_evlog () which sends msg over conn
 ********************************************/
class rwlogd_rt_sink_conn :public rwlogd_sink{
 public:
  rwlogd_rt_sink_conn() { }
  virtual ~rwlogd_rt_sink_conn() { }
 virtual int post_evlog(uint8_t *proto , rwlog_hdr_t *hdr) {RW_CRASH();return -1;}
  virtual void rwlog_dump_info(int verbose)
  {
    printf("Type : Real-time-sink\n");
    rwlogd_sink::rwlog_dump_info(verbose);
  }
};
/////////Batch processing sinks //////////////////////
/********************************************
 * Batch Sinks
 ********************************************/

class rwlogd_batch_sink_conn :public rwlogd_sink{
 public:
  int r_ptr; // Place to start reading from
  int w_ptr; // Place to end reading 
  int dir;   // Direction . forward or backward.
  virtual void post_evlog(void *event_log_list){};
  virtual void add_generated_callid_filter(uint8_t *proto)  {};
  virtual void remove_generated_callid_filter(uint64_t callid)  {};
};
class rwlogd_protoconvertor {
 public:
};

extern "C"  {
#include <tcpdump/interface.h>
  extern void ip_print(netdissect_options *ndo, const u_char *bp, u_int length);
#if 0
  extern rw_status_t rw_pb_get_field_value_uint64 (const ProtobufCMessage *pbcm, 
                                                   const char *field_name, 
                                                   uint64_t *out);
#endif

  extern rw_status_t add_exact_match_filter_string(filter_attribute  **filter_info,
                                                   char *category_str,
                                                   char *field_name,
                                                   char *value,
                                                   int next_call_flag);

  extern rw_status_t add_exact_match_deny_filter_string(filter_attribute  **filter_info,
                                                        char *field_name,
                                                        char *value);

  extern void rwlogd_sink_obj_deinit(rwlogd_tasklet_instance_ptr_t rwlogd_instance_data);

  rw_status_t rwlogd_sink_filter_operation(FILTER_ACTION action, rwlogd_instance_ptr_t instance,
                                           char *category,
                                           char *name, char *field_name, char *value,
                                           int next_call_flag);
  extern rw_status_t rwlogd_sink_filter_operation_uint64(FILTER_ACTION action,
                                                         rwlogd_instance_ptr_t instance,
                                                         char *category,
                                                         char *name, char *field_name,
                                                         uint64_t value);

  extern uint32_t rwlogd_get_num_categories(rwlogd_instance_ptr_t instance);
  extern category_str_t *rwlogd_get_category_list(rwlogd_instance_ptr_t instance);

  extern int rwlogd_map_category_string_to_index(rwlogd_instance_ptr_t instance,
                                                 char *category_str);

  extern rw_status_t rwlogd_sink_severity(rwlogd_instance_ptr_t instance,
                                          char *sink_name,
                                          char *category,
                                          rwlog_severity_t severity,
                                          RwLog__YangEnum__OnOffType__E critical_info_filter);
  extern rwlog_severity_t rwlogd_sink_get_severity(rwlogd_instance_ptr_t instance,
                                                   uint32_t category);

  extern rw_status_t
  rwlogd_sink_update_vnf_id(rwlogd_instance_ptr_t instance,
                            char *sink_name,
                            char *vnf_id);

  extern  rw_status_t
  rwlogd_handle_dynamic_schema_update(rwlogd_instance_ptr_t instance,
                                      const uint64_t batch_size,
                                      rwdynschema_module_t *modules);

  extern rw_status_t rwlogd_sink_apply_next_call_filter(rwlogd_instance_ptr_t instance, 
                                                        char *sink_name,
                                                        RwLog__YangEnum__OnOffType__E next_call_flag);

  extern rw_status_t rwlogd_sink_apply_failed_call_filter(rwlogd_instance_ptr_t instance, 
                                                          char *sink_name,
                                                          RwLog__YangEnum__OnOffType__E next_failed_call_flag,
                                                          bool next_call_filter);

  extern rw_status_t add_exact_match_filter_uint64(filter_attribute **filter_info,
                                                   char *field_name, uint64_t value);

  void rwlogd_handle_default_severity(rwlogd_instance_ptr_t rwlogd_instance,
                                      char *cat,
                                      rwlog_severity_t sev);

  extern "C" rwlogd_sink_data *rwlogd_get_sink_obj (rwlogd_instance_ptr_t rwlogd_instance);

}
#endif
