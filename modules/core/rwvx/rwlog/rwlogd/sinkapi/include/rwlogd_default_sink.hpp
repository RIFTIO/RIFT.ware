
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __RWLOGD_DEFAULT_SINK_HPP__
#define __RWLOGD_DEFAULT_SINK_HPP__

#include "rwlogd_sink_common.hpp"
#include "rwlogd_common.h"
#include<stdio.h> 
#include<string.h> 
#include<sys/socket.h>
#include<sys/queue.h>
#include<errno.h>
#include<netdb.h>
#include<arpa/inet.h>
#define IP_LEN 46
#include "rwlogd_default_sink.hpp"
#include "rwlogd_sink_common.hpp"
#include "rw_namespace.h"

//const int size_of_syslog 256;
#define CLI_MAX_SZ 4096
/********************************************
 * Default   Realtime Class
 ********************************************/

bool cmp_logts(rwlog_hdr_t *v1, rwlog_hdr_t *v2);
bool cmp_match_logts(rwlog_hdr_t *v1, rwlog_hdr_t *v2);
class rwlogd_default_sink : public rwlogd_rt_sink_conn {
private:
  Rwlogd_Client log_cli;
  typedef ring_buffer<rwlog_hdr_t *> log_ring_t;
  log_ring_t *log_ring_;
public:
  rwlogd_default_sink(const char *sink_name);
  virtual int post_evlog(uint8_t *buffer);
  virtual rwlog_status_t pre_process_evlog(uint8_t *proto);
  virtual void rwlog_dump_info(int verbose);
  void poll();
  // Log Ring APIs
  rw_status_t add_log_to_ring(uint8_t *buffer);
  void remove_log_from_ring(uint8_t *buffer);
  void clear_log_from_ring();
  void get_log_ring_stats(ring_buffer_stats_t *stats);
  rwlog_status_t match_category_severity(uint8_t *log_hdr, 
                              uint32_t filter_cat,
                              rwlogd_filter &filter);
  rwlog_status_t match_timestamp (uint8_t *buffer, 
                       struct timeval *start_time,
                       struct timeval *end_time);
  int showlog_get_string(uint8_t *proto,
                                    char *string,
                                    size_t string_sz,
                                    rwlogd_filter &filter,
                                    rwlog_msg_id_key_t  *msg_id,
                                    size_t id_string_sz,
                                    char *pdu_dump,
                                    char *pdu_hex_dump,
                                    int verbosity);
  rwlog_status_t match_cid(uint8_t *log, uint64_t cid, uint64_t cgid);
  rwlog_status_t match_msgid(uint8_t *buffer, rwlog_msg_id_key_t *msg_id_key);
  rwlog_status_t match_inactive_logs(char *buffer,char *inactive_buffer, 
                                      uint32_t inactive_size);
  rw_status_t get_log(int &pos,rwlogd_output_specifier_t *output_spec);
  rw_status_t get_position(int &pos,struct timeval *start_time);
  int get_last_position() { return log_ring_->get_last_position(); }
  rw_status_t get_log_trailing_timestamp(int pos, char *buf, size_t buf_sz);
  virtual void  add_generated_callid_filter(uint8_t *proto) { }
  rwlog_status_t apply_retrieve_filter(rwlog_hdr_t *hdr,
                                       rwlogd_output_specifier_t *output_spec,
                                       rwlogd_filter &local_filter);
  virtual void  remove_generated_callid_filter(uint64_t callid) { }
}; 
/*****************************************
 * APis to add the Sinks to the Rwlogd 
 *****************************************/
// Call for CLI 
extern "C" rw_status_t rwlogd_create_default_sink(rwlogd_instance_ptr_t inst, const char *name);
extern "C" rw_status_t rwlogd_delete_default_sink(rwlogd_instance_ptr_t inst, const char *sink_name);
extern "C" rw_status_t rwlogd_remove_log_from_default_sink(rwlogd_instance_ptr_t inst, rwlog_hdr_t *log_hdr);
extern "C" void rwlogd_clear_log_from_default_sink(rwlogd_instance_ptr_t inst);
#endif
