
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

#ifndef __RWLOGD_FILTERS_H__
#define __RWLOGD_FILTERS_H__

#include <list>
#include "rwlogd_common.h"
#include "rw-log.pb-c.h"
#include "yangncx.hpp"
#include "rwlogd_utils.h"
/*******************************************************************************
        Class: rwlogd_sink_data 
        ========================
        Exists as void* in instance data
        Maintains All the Sink queues. 
        Maintains All Eventlog circular buffer for the queus
        Maintains the filter shared memory.
        Maintains the Yang data. 
 ******************************************************************************/
class rwlogd_sink;
enum rwlog_status_t
{
  RWLOG_FILTER_MATCHED,
  RWLOG_CHECK_MORE_FILTERS,
  RWLOG_FILTER_DROP,
};


extern "C" rw_status_t add_exact_match_filter_string(filter_attribute **filter_info,
                                                     char *category_str,
                                                     char *field_name,
                                                     char *value,
                                                     int next_call_flag);
filter_attribute* find_exact_match_filter_string(filter_attribute **filter_info,
                                                 char *category_str,
                                                 const char *field_name,
                                                 const char *value);
rw_status_t remove_exact_match_filter_string(filter_attribute **filter_info,
                                             char *category_str,
                                             char *field_name,
                                             char *value);

extern "C" rw_status_t add_exact_match_deny_filter_string(filter_attribute **deny_filter_info,
                                    char *field_name,
                                    char *value);
extern "C" rw_status_t add_exact_match_filter_uint64(filter_attribute **filter_info,
                                    char *field_name,
                                    uint64_t value);
extern "C" rw_status_t add_exact_match_filter_uint64_new(filter_attribute **filter_info,
                                    uint32_t cat,
                                    char *field_name,
                                    uint64_t value);
rw_status_t remove_exact_match_filter_uint64_new(filter_attribute **filter_info,
                                       uint32_t cat,
                                       char *field_name,
                                       uint64_t value);
rw_status_t remove_exact_match_filter_uint64(filter_attribute **filter_info,
                                       char *field_name,
                                       uint64_t value);

rwlog_status_t find_exact_match_deny_filter_string(filter_attribute **deny_filter_info,
						   const char *field_name,
						   const char *value);
rwlog_status_t find_exact_match_filter_uint64(filter_attribute **filter_info,
                                        const char *field_name,
                                        uint64_t value);
rw_status_t find_exact_match_filter_uint64_new(filter_attribute **filter_info,
                                        uint32_t cat,
                                        const char *field_name,
                                        uint64_t value);
class rwlogd_filter {
 public:
  filter_memory_header filter_hdr_;
  rwlog_category_filter_t *category_filter_;
  filter_attribute *filter_info; //  Used for local filtering Sinks and SinkOBJ
  filter_attribute *deny_filter_info; // Used for deny event-ID filtering for Sinks
  int num_filters;
  bool protocol_filter_set_;
  rwlogd_filter();
  ~rwlogd_filter();
  void rwlog_dump_info(int verbosity);


  //Severity based
  rwlog_severity_t get_severity(uint32_t category);
  void set_severity(uint32_t category,
                    rwlog_severity_t sev);
  bool check_severity(rwlog_severity_t sev, uint32_t cat);

  bool check_critical_info_filter(uint16_t critical_info_flag, uint32_t cat);
  void set_critical_info_filter(uint32_t category,RwLog__YangEnum__OnOffType__E critical_info_filter);


  void set_protocol_filter(uint32_t protocol, bool enable);
  bool is_protocol_filter_set(uint32_t protocol);
  bool is_protocol_filter_flag_set() { return protocol_filter_set_; }

  bool is_packet_direction_match(uint32_t direction);

  // Call based
  void update_next_call_filter(bool enable) { filter_hdr_.next_call_filter = enable; }
  bool get_next_call_filter() { return filter_hdr_.next_call_filter; }
  void update_failed_call_filter(bool enable,bool next_call_filter) { filter_hdr_.failed_call_filter = enable; next_failed_call_filter= next_call_filter; }
  bool get_failed_call_filter() { return filter_hdr_.failed_call_filter; }
  bool get_next_failed_call_filter() { return next_failed_call_filter; }

  bool check_string_filter(char *cat_str, uint32_t cat,
                           char *name, char *value);
  bool is_attribute_filter_set(uint32_t cat);
  bool check_deny_event_filter(const char *name, const char *value);
  bool check_groupid_filter(char *name, rw_call_id_t *value);
  bool check_uint64_filter(const char *name, uint64_t value);
  bool check_int_filter(const char *name, int value);
  struct filter_stats {
    uint64_t str_filter_no_bitmap;
    uint64_t str_filter_matched;
    uint64_t str_filter_nomatch;
    uint64_t severity_check_passed;
    uint64_t severity_check_failed;
    void rwlog_dump_stats(int verbose)
    {
      printf("Filter Op Statistics:\n");
      printf("\tstr_filter_no_bitmap %lu\n", str_filter_no_bitmap);
      printf("\tstr_filter_matched %lu\n", str_filter_matched);
      printf("\tstr_filter_nomatch %lu\n", str_filter_nomatch);
      printf("\tseverity_check_passed %lu\n", severity_check_passed);
      printf("\tseverity_check_failed %lu\n", severity_check_failed);
    }
  };
  static filter_stats stats;
 private:
  bool next_failed_call_filter;
};

#endif
