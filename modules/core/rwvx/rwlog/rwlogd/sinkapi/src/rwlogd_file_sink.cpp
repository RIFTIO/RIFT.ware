
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

#include "rwlogd_sink_common.hpp"
#include <syslog.h>
/********************************************
 * File Class
 ********************************************/
#define FILELOG_MAX_SZ 4096

class file_conn : public rwlogd_rt_sink_conn {
private:
  int _file_fd;
  char *_filename;
  int _lead_instance_id;
  char *cli_dest_path;
  rwmsg_destination_t *dest;
  rwmsg_clichan_t *log_cli_chan;
  RwlogdPeerAPI_Client *log_cli;
  uint64_t logs_from_peer;
  char _is_lead_instance;

public:
  file_conn(const char *sink_name, 
              const char *filename, int lead_instance_id,
              void *connection_params = NULL)
  {
    rwlogd_instance_ptr_t inst_data = (rwlogd_instance_s*)connection_params;
    strncpy(sink_name_, sink_name,SINKNAMESZ);
    _lead_instance_id = lead_instance_id;
    _file_fd  = -1;
    dest = NULL;
    log_cli_chan = NULL;
    log_cli = NULL;
    logs_from_peer = 0;
    cli_dest_path = NULL;
    if(filename[0] == '/') {
      asprintf(&_filename,
               "%s",filename);
    }else { 
      asprintf(&_filename,
               "/tmp/%s",filename);
    }
    if(_lead_instance_id == inst_data->rwtasklet_info->identity.rwtasklet_instance_id || _lead_instance_id == 0) {
      _is_lead_instance = TRUE;
      _file_fd = open(_filename,O_RDWR|O_CREAT|O_APPEND|O_CLOEXEC,S_IRWXU);
      if(_file_fd > 0) {
        set_state (CONNECTED);
      }
      else {
        fprintf(stderr,"Opening file %s failed with error %s\n",_filename,strerror(errno));
        set_state(INIT);
      }
    }
    else {
      RW_ASSERT(inst_data); 
      if (!inst_data) { return; }

      _is_lead_instance = FALSE;
      log_cli_chan = inst_data->cc;
      log_cli = &inst_data->rwlogd_peer_msg_client; 
      int r = asprintf (&cli_dest_path, "/R/%s/%d", RWLOGD_PROC, lead_instance_id);
      RW_ASSERT(r);
      if (!r) { return; }

      dest = rwmsg_destination_create(
          inst_data->rwtasklet_info->rwmsg_endpoint,
          RWMSG_ADDRTYPE_UNICAST,
          cli_dest_path);
        set_state (CONNECTED);
    }
  }
  virtual ~file_conn()
  {
    if(_file_fd > 0)
    {
      close(_file_fd);
    }
    if(dest) {
      rwmsg_destination_release(dest);
    }
    if(cli_dest_path) {
      free (cli_dest_path);
    }
    RW_FREE(_filename);
  }
  virtual int post_evlog(uint8_t *proto )
  {
    char filelog[FILELOG_MAX_SZ];
    size_t n = 0;
    if(convert_log_to_str(proto,filelog, FILELOG_MAX_SZ) > 0)
    {
      log_filter_matched++;

      if(_is_lead_instance) {
        size_t len = strlen(filelog);;
        filelog[len] = '\n';
        n = write(_file_fd,filelog,len+1);
        if(n != len+1) {
          fprintf(stderr,"Write to log file %s of size %lu failed; written size is: %lu, error: %s\n",
                  _filename,len+1,n,strerror(errno));
        }
      }
      else {
        rw_status_t status = RW_STATUS_SUCCESS;
        RwlogdFileSendLogReq req;
        rwlogd_file_send_log_req__init(&req);
        req.my_key = strdup(filelog);
        req.sink_name = sink_name_;
        rwmsg_closure_t clo = { {.pbrsp = NULL} ,.ud = NULL };
        status = rwlogd_peer_api__file_send_log(log_cli, dest, &req, &clo, NULL);
        RW_ASSERT (status != RW_STATUS_FAILURE);
        free(req.my_key);
      }
    }
    return n;
  }
  virtual void handle_file_log(char *log_string)
  {
    logs_from_peer++;
    if(_file_fd) {
      size_t len = strlen(log_string);;
      log_string[len] = '\n';
      size_t n = write(_file_fd,log_string,len+1);
      if(n != len+1) {
        fprintf(stderr,"Write to log file %s of size %lu failed; written size is: %lu, error: %s\n",
                _filename,len+1,n,strerror(errno));
      }
    }
    else {
      fprintf(stderr,"FD is invalid for sink %s",sink_name_);
    }
  }
  void poll()
  {
  }
private:
  virtual void rwlog_dump_info(int verbose)
  {
    printf ("File Name %s\n", _filename);
    printf ("FD = %d \n", _file_fd);
    printf("Lead Instance = %d\n",_lead_instance_id);
    printf("Logs from peer: %lu\n",logs_from_peer);
    rwlogd_rt_sink_conn::rwlog_dump_info(verbose);
  }
}; 

class bootstrap_syslog_conn : public rwlogd_rt_sink_conn 
{

public:
  bootstrap_syslog_conn( void *connection_params=NULL,rwlog_severity_t severity=RW_LOG_LOG_SEVERITY_ERROR)
  {
    strncpy(sink_name_, BOOTSTRAP_SYSLOG_SINK, SINKNAMESZ);
    openlog(NULL, LOG_PID|LOG_CONS, LOG_USER);
    set_state (CONNECTED);
    bootlog_severity_ = severity;
  }
  virtual ~bootstrap_syslog_conn()
  {
    closelog();
  }
  rwlog_status_t pre_process_evlog(uint8_t *proto)
  {
    rwlog_hdr_t *log_hdr= (rwlog_hdr_t *)proto;
    /* Allow logs with bootstrap filter severity */
    if(bootlog_severity_ >= (rwlog_severity_t)log_hdr->log_severity ||
       log_hdr->critical_info) {
      return RWLOG_FILTER_MATCHED;
    }
    return RWLOG_FILTER_DROP;
  }
  virtual int post_evlog(uint8_t *proto )
  {
    char filelog[FILELOG_MAX_SZ];
    size_t len = 0;
    if(convert_log_to_str(proto,filelog, FILELOG_MAX_SZ,TRUE) > 0)
    {
      log_filter_matched++;
      len = strlen(filelog);;
      filelog[len] = '\n';
      syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "%s", filelog);
    }
    return len;
  }
  void poll()
  {
  }
  virtual void add_generated_callid_filter(uint8_t *proto)  { }
  virtual void remove_generated_callid_filter(uint64_t callid) { }
private:
  rwlog_severity_t  bootlog_severity_;
  virtual void rwlog_dump_info(int verbose)
  {
    printf("Default sev = %d\n",bootlog_severity_);
    rwlogd_rt_sink_conn::rwlog_dump_info(verbose);
  }
}; 

class pcap_conn : public rwlogd_rt_sink_conn {
private:
  char *filename_;
  int lead_instance_id_;

public:
  pcap_conn(const char *sink_name, 
              const char *filename, int lead_instance_id,
              void *connection_params = NULL)
  {
    strncpy(sink_name_, sink_name,SINKNAMESZ);
    filename_ = RW_STRDUP(filename);
    asprintf(&filename_,
              "%s-%d.pcap",filename,rwlog_get_systemId());
    lead_instance_id_ = lead_instance_id;

    FILE *fp_ = fopen(filename_, "w");
    struct pcap_file_header file_header;
    #define TCPDUMP_MAGIC     0xa1b2c3d4
    #ifdef snaplen
    #undef snaplen
    #endif
  
    if (!fp_) {
      set_state (INIT);
      return;
    }
    file_header.magic = TCPDUMP_MAGIC;
    file_header.version_major  =  (PCAP_VERSION_MAJOR);
    file_header.version_minor =  (PCAP_VERSION_MINOR);
    file_header.thiszone = 0;
    file_header.sigfigs = 0;
    file_header.snaplen = (65535);
    file_header.linktype =  (1); //www.tcpdump.org/linktypes.html

    fwrite(&file_header,(size_t)sizeof(file_header),(size_t)1,fp_); 
    fclose(fp_);
    set_state (CONNECTED);
  }

  virtual ~pcap_conn()
  {
    RW_FREE(filename_);
  }

  virtual int post_evlog(uint8_t *proto )
  {
    rwlog_hdr_t *log_hdr = (rwlog_hdr_t *) proto;
    if (!log_hdr->cid_filter.cid_attrs.binary_log) { return 0; }

    uint32_t n = 0;
    FILE *fp_ = fopen(filename_, "a");
  
    if (!fp_) { 
      return 0;
    }

    ProtobufCMessage *log = rwlogd_sink_data::get_unpacked_proto(proto);
    if (!log) {
      fclose(fp_);
      return 0;
    }
    /// loop for attributes
    const ProtobufCMessageDescriptor *log_desc = log->descriptor;
    const ProtobufCFieldDescriptor *fd = nullptr;
    void *field_ptr = nullptr;
    size_t off = 0;
    protobuf_c_boolean is_dptr = false;

    for (unsigned i = 0; i < log_desc->n_fields; i++)
    {
      fd = &log_desc->fields[i];

      field_ptr = nullptr;
      int count = protobuf_c_message_get_field_count_and_offset(log, fd, &field_ptr, &off, &is_dptr);
      if (count && fd->type == PROTOBUF_C_TYPE_MESSAGE && field_ptr && !strcmp(fd->name, "pkt_info"))
      {
        pkt_trace_t *pkt_info = (pkt_trace_t *)field_ptr;
        n= pkt_info->packet_data.len;
        
        fwrite(&log_hdr->tv_sec,(size_t)sizeof(log_hdr->tv_sec),(size_t)1,fp_); 
        fwrite(&log_hdr->tv_usec,(size_t)sizeof(log_hdr->tv_usec),(size_t)1,fp_); 
        fwrite(&n,(size_t)sizeof(n),(size_t)1,fp_);
        fwrite(&n,(size_t)sizeof(n),(size_t)1,fp_);
        fwrite(pkt_info->packet_data.data,(size_t)n,(size_t)1,fp_); 

        log_filter_matched++;
      }
    } //for loop

    fclose(fp_);
    rwlogd_sink_data::free_proto(log);
    return n;
  }

private:
  virtual void rwlog_dump_info(int verbose)
  {
    printf ("File Name %s\n", filename_);
    printf("Lead Instance = %d\n",lead_instance_id_);
    rwlogd_rt_sink_conn::rwlog_dump_info(verbose);
  }
}; 

/*****************************************
 * APis to add the Sinks to the Rwlogd 
 *****************************************/
extern "C" rw_status_t start_rwlogd_file_sink(rwlogd_instance_ptr_t inst, 
                                                 const char *name, 
                                                 const char *filename,int lead_instance_id)
{
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(name);    
  if (sink) 
  {   
    RWLOG_FILTER_DEBUG_PRINT("SINK MATCHED****** %s\n", name);
    return RW_STATUS_DUPLICATE;
  }   
  file_conn *conn = new file_conn(name , filename,lead_instance_id,inst);
  obj->add_sink (conn);
  return RW_STATUS_SUCCESS;
}


extern "C" rw_status_t stop_rwlogd_file_sink(rwlogd_instance_ptr_t inst, 
                                                 const char *name, 
                                                 const char *filename)
{
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *conn = obj->remove_sink(name);
  if (conn)
  {
    delete (conn);
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_NOTFOUND;
}

extern "C" rw_status_t start_rwlogd_pcap_sink(rwlogd_instance_ptr_t inst, 
                                                 const char *name, 
                                                 const char *pcap_file,int lead_instance_id)
{
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(name);    
  if (sink) 
  {   
    RWLOG_FILTER_DEBUG_PRINT("SINK MATCHED****** %s\n", name);
    return RW_STATUS_DUPLICATE;
  }   
  pcap_conn *conn = new pcap_conn(name , pcap_file ,lead_instance_id,inst);
  obj->add_sink (conn);
  return RW_STATUS_SUCCESS;
}


extern "C" rw_status_t stop_rwlogd_pcap_sink(rwlogd_instance_ptr_t inst, 
                                                 const char *name, 
                                                 const char *pcap_file)
{
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *conn = obj->remove_sink(name);
  if (conn)
  {
    delete (conn);
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_NOTFOUND;
}

/*!
 * add a bootstrap syslog sink - used for debugging A place to look at logs
 * when the system itself did not boot properly.
 * invoked at the time of initialization of system
 * @input : rwlogd_instance_ptr_t - rwlgod instance pointer
 * @return : RW_STATUS_SUCCESS or RW_STATUS_DUPLICATE
 */
extern "C" rw_status_t start_bootstrap_syslog_sink(rwlogd_instance_ptr_t inst) 
{
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(BOOTSTRAP_SYSLOG_SINK);    
  if (sink) 
  {   
    RWLOG_FILTER_DEBUG_PRINT("SINK MATCHED****** %s\n", sink->get_name());
    return RW_STATUS_DUPLICATE;
  }   
  bootstrap_syslog_conn *conn = new bootstrap_syslog_conn(inst,inst->rwlogd_info->bootstrap_severity);
  obj->add_sink (conn);
  return RW_STATUS_SUCCESS;
}


/*!
 * delete bootstrap syslog sink - A place to look at logs
 * when the system itself did not boot properly.
 * Invoked after boostap period is complete.
 * @input : rwlogd_instance_ptr_t - rwlgod instance pointer
 * @return : RW_STATUS_SUCCESS or RW_STATUS_NOTFOUND
 */
extern "C" rw_status_t stop_bootstrap_syslog_sink(rwlogd_instance_ptr_t inst)
{
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *conn = obj->remove_sink(BOOTSTRAP_SYSLOG_SINK);
  if (conn)
  {
    delete (conn);
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_NOTFOUND;
}

