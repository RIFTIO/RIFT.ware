
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include "rwlogd_sink_common.hpp"
/********************************************
 * File Class
 ********************************************/

#define LOG_MAX_SZ 4096
class console_conn : public rwlogd_rt_sink_conn {
private:
  FILE *_fileh;
  uint32_t _default_severity = RW_LOG_LOG_SEVERITY_ERROR;

public:
  console_conn(const char *sink_name,rwlog_severity_t default_severity)
  {
    strncpy(sink_name_, sink_name,SINKNAMESZ);
    set_state (CONNECTED);
    _fileh = fdopen(STDOUT_FILENO, "a");
    setvbuf(_fileh, NULL, _IOFBF, 2048);
    char *env = getenv("RIFT_CONSOLE_LOG_SEVERITY");
    if(env) {
      _default_severity = atoi(env);
    }else {
      _default_severity = default_severity;
    }
  }
  virtual ~console_conn()
  {
    _fileh = NULL;
  }

  virtual int handle_evlog(uint8_t *buffer) {
    bool allow_log = (((rwlog_hdr_t *)buffer)->log_severity <= _default_severity);
    if(allow_log == TRUE || pre_process_evlog(buffer) != RWLOG_FILTER_DROP) {
      char filelog[LOG_MAX_SZ];
      if(convert_log_to_str(buffer,filelog, LOG_MAX_SZ,allow_log) > 0)
      {
        log_filter_matched++;
        fprintf(stdout,"%s\n",filelog);
        //fprintf(_fileh, "%s\n",filelog);
      }
    }
    return 0;
  }

  virtual void add_generated_callid_filter(uint8_t *proto)
  {
    add_session_callid_filter(proto,sink_filter_);
  }
  virtual void remove_generated_callid_filter(uint64_t callid)
  {
    remove_session_callid_filter(callid,sink_filter_);
  }
  void poll()
  {
  }
private:
  virtual void rwlog_dump_info(int verbose)
  {
    printf ("FD = %lx \n",(uint64_t) _fileh);
    printf("Default sev = %d\n",_default_severity);
    rwlogd_rt_sink_conn::rwlog_dump_info(verbose);
  }
}; 

/*****************************************
 * APis to add the Sinks to the Rwlogd 
 *****************************************/
extern "C" rw_status_t start_rwlogd_console_sink(rwlogd_instance_ptr_t inst, 
                                                 const char *name)
{
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(name);    
  if (sink) 
  {   
    RWLOG_FILTER_DEBUG_PRINT("SINK MATCHED****** %s\n", name);
    return RW_STATUS_DUPLICATE;
  }   
  console_conn *conn = new console_conn(name,inst->rwlogd_info->default_console_severity);
  obj->add_sink (conn);
  return RW_STATUS_SUCCESS;
}


extern "C" rw_status_t stop_rwlogd_console_sink(rwlogd_instance_ptr_t inst, 
                                                 const char *name)
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
