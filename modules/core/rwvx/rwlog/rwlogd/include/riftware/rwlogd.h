
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __RWLOGD_H__
#define __RWLOGD_H__


#include <rw_tasklet_plugin.h>
#include "rwtasklet.h"
#include "rwlogd.pb-c.h"
#include "rwlog-mgmt.pb-c.h"
#include <uuid/uuid.h>
#include "rw-mgmt-schema.pb-c.h"
#include "rwdynschema.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "rwsystem_timer.h"

#ifndef MAX_PDU_SIZE
#define MAX_PDU_SIZE 4096
#endif

  typedef enum {
    FILTER_ADD,
    FILTER_DEL,
    FILTER_MOD,
    FILTER_LKUP,
    FILTER_DENY_EVID,
  } FILTER_ACTION;

  struct rwlogd_component_s {
    CFRuntimeBase _base;
    /* ADD ADDITIONAL FIELDS HERE */
  };
  typedef struct {
    int       fd;
    struct stat sd;
    uint64_t ticks;
  }file_data_t;

  /* Kinda short, mainly to test rotation. */
#define RWLOGD_RX_ROTATE_TIME_S (2*60)   

#define RWLOGD_MAX_READ_PER_ITER  5000  /* 5k logs per iter */


#define CIRCULAR_BUFFER_FACTOR 10
#define CLI_MAX_LOG_LINES 2048
#define RWLOGD_LOG_LINE_LEN 1024
#define MD5STRINGSZ 256
#define RWLOGD_LOG_MSG_ID_LEN 128
#define RWLOG_MGMT_FILTER_SZ 256
#define RWLOG_MGMT_DTS_TPATH_SZ 257
#define RWLOG_MGMT_PCAP_SNAPLEN 65535 

  //10 MB
#define RWLOGD_MAX_FILE_SIZE 10*1024*1024 

  /* 25 MB - changed as part of JIRA 4078 as now full circular buffer is used */
#define CIRCULAR_BUFFER_SIZE 25*1024*1024 
  // Should be per APP affair
  // The main logic is there can be two files
  // 1. Actively written 
  // 2. Actively read

  typedef struct {  
    int file_current_index; // To detect rotation
    int rotation_in_progress; // Not needed by App
    int tokens; /// For backpresssure.
  }rwlogd_file_feedback_t;

  typedef struct {
    uint64_t log_invalid_magic;
    uint64_t file_rotations;
    uint64_t circ_buff_wrap_around;
    uint64_t circ_buff_wrap_failure;
    uint64_t log_hdr_validation_failed;
    uint64_t chkpt_stats;
    uint64_t invalid_log_from_peer;
    uint64_t log_forwarded_to_peer;
    uint64_t logs_received_from_peer;
    uint64_t sending_log_to_peer_failed;
    uint64_t peer_send_requests;
    uint64_t peer_recv_requests;
    uint64_t multiple_file_rotations_atttempted;
  } rwlogd_stats_t;

  typedef struct {
    int rwtasklet_instance_id;
    char *rwtasklet_name;
    rw_sklist_element_t           element;
    uint8_t log_buffer[4096];
    uint32_t current_size;
  }rwlogd_peer_node_entry_t;

  struct rwlogd_tasklet_instance_s {
    rwsched_dispatch_source_t connection_queue_timer;
    rwsched_dispatch_queue_t connection_queue;
    void *sink_data;
    void *rwlogd_instance;
    file_data_t file_status;
    char *rwlog_filename;
    int rotation_in_progress;
    uint32_t bootstrap_counter;
    uint32_t bootstrap_time; /*Bootstrap time from manifest */
    rwlog_severity_t bootstrap_severity; /* Bootstrap log severity level */
    int bootstrap_completed;
    rwlog_severity_t default_console_severity;
    void *rwlogd_list_ring;
    rw_sklist_t    peer_rwlogd_list;
    rwlogd_file_feedback_t *rwlogfile_feedback;
    uint32_t num_categories;
    uint32_t curr_used_offset;
    uint32_t curr_offset;
    uint32_t log_buffer_size;
    char *log_buffer;
    char *log_buffer_chkpt;
    char hostname[MAX_HOSTNAME_SZ];
    rwlogd_stats_t stats;
    rwsched_dispatch_source_t timer;
    stw_t *stw_system_handle;
  };

  RW_TYPE_DECL(rwlogd_tasklet_instance);
  RW_CF_TYPE_EXTERN(rwlogd_tasklet_instance_ptr_t);

  RW_TYPE_DECL(rwlogd_component);

  struct rwlogd_instance_s {
    CFRuntimeBase _base;
    rwtasklet_info_ptr_t rwtasklet_info;
    rwmsg_srvchan_t *sc;
    rwmsg_clichan_t *cc;
    Rwlogd_Service rwlogd_srv;
    Rwlogd_Client rwlogd_cli;
    RwlogdPeerAPI_Client rwlogd_peer_msg_client;
    RwlogdPeerAPI_Service rwlogd_peerapi_srv;
    void *dts_h; 
    void *dynschema_reg_handle;
    RwMgmtSchema__YangEnum__ApplicationState__E dynschema_app_state;
    void       *lead_update_dts_member_handle;
    char *instance_name;
    rwlogd_tasklet_instance_ptr_t rwlogd_info;
    rwsched_CFRunLoopTimerRef stop_cftimer;
    pid_t core_monitor_script_child;
  };
  RW_TYPE_DECL(rwlogd_instance);

  typedef struct log_bpf_inst
  {
    uint16_t code;
    uint8_t jt;
    uint8_t jf;
    uint32_t k;
  } log_bpf_inst_t;

  typedef struct log_bpf_filter {
    uint32_t bf_len;
    log_bpf_inst_t bpf_inst[256];
  } log_bpf_filter_t;

  typedef struct msg_id_key  {
    uint32_t tv_sec;
    uint32_t tv_usec;
    char hostname[MAX_HOSTNAME_SZ];
    uint32_t pid;
    uint32_t tid;
    uint64_t seqno;
  } rwlog_msg_id_key_t;

  typedef struct {
    struct timeval start_time;
    struct timeval end_time;
    void *show_filter_ptr;
    uint32_t cat;
    uint64_t callid;
    uint64_t callgroupid;
    uuid_t vnf_id;
    char log_line[RWLOGD_LOG_LINE_LEN];
    //char msg_id[RWLOGD_LOG_MSG_ID_LEN];
    rwlog_msg_id_key_t msg_id;
    bool has_input_msg_id;
    rwlog_msg_id_key_t input_msg_id;
    char pdu_dump[MAX_PDU_SIZE];
    char pdu_hex_dump[MAX_PDU_SIZE];
    int  verbosity;
    bool has_inactive_logs;
    char* inactive_log_buffer;
    uint32_t inactive_log_size;
  } rwlogd_output_specifier_t;

  rwlogd_component_ptr_t rwlogd_component_init(void);

  void rwlogd_component_deinit(rwlogd_component_ptr_t component);

  rwlogd_instance_ptr_t rwlogd_instance_alloc(
      rwlogd_component_ptr_t component, 
      struct rwtasklet_info_s * rwtasklet_info, 
      RwTaskletPlugin_RWExecURL *instance_url);

  void rwlogd_instance_free(
      rwlogd_component_ptr_t component, 
      rwlogd_instance_ptr_t instance);

  void rwlogd_instance_start(
      rwlogd_component_ptr_t component, 
      rwlogd_instance_ptr_t instance);
  void rwlogd_start_tasklet(rwlogd_instance_ptr_t instance, bool dts_reg,char *rwlog_filename,char *filter_shm_name,
                            const char *schema_name);

  rw_status_t rwlog_allow_dup_events( rwlogd_instance_ptr_t instance);
  extern rw_status_t rwlogd_shm_filter_operation_uint64(FILTER_ACTION action,
                                                        rwlogd_instance_ptr_t instance,
                                                        char *field_name,
                                                        uint64_t value);
  extern rw_status_t rwlogd_sink_filter_operation(FILTER_ACTION action,
                                                  rwlogd_instance_ptr_t instance,
                                                  char *category,
                                                  char *name,
                                                  char *field_name,
                                                  char *value,
                                                  int next_call_flag);

  extern rw_status_t rwlogd_sink_filter_operation_uint64(FILTER_ACTION action,
                                                         rwlogd_instance_ptr_t instance,
                                                         char *category,
                                                         char *name,
                                                         char *field_name,
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

  rw_status_t rwlogd_sink_update_protocol_filter(rwlogd_instance_ptr_t instance,
                                                 char *sink_name,
                                                 RwLog__YangEnum__ProtocolType__E protocol,
                                                 bool enable);

  extern rwlog_severity_t rwlogd_sink_get_severity(rwlogd_instance_ptr_t instance,
                                                   uint32_t category);

  extern rw_status_t
  rwlogd_sink_update_vnf_id(rwlogd_instance_ptr_t instance,
                            char *sink_name,
                            char *vnf_id);
  extern rw_status_t
  rwlogd_sink_delete(rwlogd_instance_ptr_t instance,
                            char *sink_name);

  extern  rw_status_t
  rwlogd_handle_dynamic_schema_update(rwlogd_instance_ptr_t instance,
                                      const uint64_t batch_size,
                                      rwdynschema_module_t *modules);

  extern rw_status_t rwlogd_sink_apply_next_call_filter(rwlogd_instance_ptr_t instance, 
                                                        char *sink_name,
                                                        RwLog__YangEnum__OnOffType__E next_call_flag);

  extern rw_status_t rwlogd_sink_apply_failed_call_filter(rwlogd_instance_ptr_t instance, 
                                                          char *sink_name,
                                                          RwLog__YangEnum__OnOffType__E failed_call_flag,
                                                          bool next_call_filter);

  extern rw_status_t add_exact_match_filter_string(filter_attribute **filter_info,
                                                   char *category_str,
                                                   char *field_name, 
                                                   char *value,
                                                   int next_call_flag);

  extern rw_status_t remove_exact_match_filter_string(filter_attribute **filter_info,
                                                      char *category_str,
                                                      char *field_name, char *value);

  extern rw_status_t add_exact_match_filter_uint64(filter_attribute **filter_info ,
                                                   char *field_name, uint64_t value);

  extern rw_status_t remove_exact_match_filter_uint64(filter_attribute **filter_info,
                                                      char *field_name, uint64_t value);

  void rwlogd_handle_default_severity(rwlogd_instance_ptr_t, char *, rwlog_severity_t);

  rw_status_t rwlogd_get_log_timestamp(rwlogd_instance_ptr_t inst, int position, struct timeval *tv, char *buf, size_t buf_sz);
  rw_status_t rwlogd_get_log_trailing_timestamp(rwlogd_instance_ptr_t inst, int position, char *buf, size_t buf_sz);
  rw_status_t rwlogd_get_log(rwlogd_instance_ptr_t inst, int *location,
                             rwlogd_output_specifier_t *output_spec);
  void *rwlogd_prepare_sink(rwlogd_instance_ptr_t inst);
  rw_status_t rwlogd_get_position(void *sink_obj, int *position,
                                  struct timeval  *start_time);
  int rwlogd_get_last_log_position(void *sink_obj);

  void *rwlogd_create_show_log_filter(rwlogd_instance_ptr_t inst,rwlog_severity_t default_severity,
                                      RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *filter_cfg);

  rw_status_t rwlogd_delete_show_log_filter(rwlogd_instance_ptr_t inst,
                                            void *show_filter);

  void rwlogd_dump_task_info(rwlogd_tasklet_instance_ptr_t inst, int instance_id, int v);

  rw_status_t rwlog_mgmt_fetch_logs(rwlogd_instance_ptr_t instance,
                                    RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input,
                                    RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log);

  rw_status_t rwlog_apply_filter_callgroup(rwlogd_instance_ptr_t instance,
                                           char *sink_name,
                                           RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *filter);

  rw_status_t 
  rwlog_apply_all_categories_filter(rwlogd_instance_ptr_t instance,
                                    char *sink_name,
                                    RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *category);

  void rwlogd_shm_reset_bootstrap(rwlogd_tasklet_instance_ptr_t inst_data);

  /*!
   * This function converts string form event msg-id into c structure
   * @param [in] msg_id_string - input string form msg-id
   * @param [out] msg_id - pointer to rwlog_msg_id_key_t structure
   * @result - RW_STATUS_SUCCESS upon successful parsing of message id
   *
   */
  rw_status_t atoc_msgid(char *msg_id_string, rwlog_msg_id_key_t *msg_id);

  void 
  rwlog_apply_default_verbosity(rwlogd_instance_ptr_t instance, rwlog_severity_t sev, 
                                char *cat);

  rw_status_t 
  rwlog_apply_session_params_filter(rwlogd_instance_ptr_t instance,
                                    char *sink_name,
                                    RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_SessionParams) *session_params,
                                    bool has_next_call_flag,
                                    RwLog__YangEnum__OnOffType__E next_call_flag);

  rw_status_t 
  rwlog_apply_next_call_filter(rwlogd_instance_ptr_t instance,
                               char *sink_name,
                               RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *config_filter);

  rw_status_t 
  rwlog_apply_failed_call_recording_filter(rwlogd_instance_ptr_t instance,
                                           char *sink_name,
                                           RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *config_filter);

  rw_status_t start_rwlogd_file_sink(rwlogd_instance_ptr_t inst, 
                                     const char *name, 
                                     const char *filename,int lead_instance_id);

  rw_status_t start_rwlogd_pcap_sink(rwlogd_instance_ptr_t inst, 
                                     const char *name, 
                                     const char *pcap_file, int lead_instance_id);

  rw_status_t start_rwlogd_console_sink(rwlogd_instance_ptr_t inst, 
                                        const char *name);

  rw_status_t stop_rwlogd_file_sink(rwlogd_instance_ptr_t inst, 
                                    const char *name, 
                                    const char *filename);

  rw_status_t stop_rwlogd_pcap_sink(rwlogd_instance_ptr_t inst, 
                                    const char *name, 
                                    const char *pcap_file);

  rw_status_t stop_rwlogd_console_sink(rwlogd_instance_ptr_t inst, 
                                       const char *name);


  void rwlogd_clear_log_from_sink(rwlogd_instance_ptr_t instance);
  /*!
   * During bootstrap period we send logs to local syslogd 4 debugging 
   * APIs start/stop bootstrap logs are invoked for this purpose
   */
  rw_status_t start_bootstrap_syslog_sink(rwlogd_instance_ptr_t inst); 
  rw_status_t stop_bootstrap_syslog_sink(rwlogd_instance_ptr_t inst); 
  /*!
   * log checkpointing API called by mgmt
   * @param inst - rwlogd instance pointer
   * @return - success on return
   *
   */
  rw_status_t rwlogd_chkpt_logs(rwlogd_instance_ptr_t inst);

  void rwlogd_add_node_to_list(rwlogd_instance_ptr_t instance, int peer_rwlogd_instance);

  /* Consistent hash related APIs */
#define RWLOGD_DEFAULT_NODE_REPLICA 4
  extern void *rwlogd_create_consistent_hash_ring(unsigned int replicas);
  extern void *rwlogd_delete_consistent_hash_ring(void *consistent_ring);
  extern void rwlogd_add_node_to_consistent_hash_ring(void *consistent_ring,char *node_name);
  extern void rwlogd_remove_node_from_consistent_hash_ring(void *consistent_ring,char *node_name);
  extern char * rwlogd_get_node_from_consistent_hash_ring_for_key(void *consistent_ring,uint64_t key);
  extern char *rwlogd_display_consistent_hash_ring(void *consistent_ring);


#ifdef __cplusplus
}
#endif

#endif //__RWLOGD_H__
