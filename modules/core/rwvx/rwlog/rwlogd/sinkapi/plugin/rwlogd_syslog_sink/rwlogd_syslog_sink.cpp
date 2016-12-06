
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

#include "rwlogd_syslog_sink.hpp"
#include "rwlogd_sink_common.hpp"
#include "syslog.h"
/********************************************
 * Syslog Class
 ********************************************/
enum connection_type {
  UDP,
  TCP,
  SCTP
};
typedef struct {
  connection_type _type;
} syslog_connection_params_t;

class syslog_conn : public rwlogd_rt_sink_conn {
private:
  int _socket_descriptor = -1;
  char *_syslog_server;
  uint32_t _port;
  struct in_addr _server_ip;
  syslog_connection_params_t _connection_params;

public:
  syslog_conn(const char *sink_name, 
              const char *hostname,
              uint32_t port,
              void *connection_params = NULL)
  {
    _syslog_server = 0;
    strncpy(sink_name_, sink_name,SINKNAMESZ);
    set_state (INIT);
    if (set_server_ip(hostname) == RW_STATUS_FAILURE)
    {
      retry ();
      return;
    }
    if (connect() == RW_STATUS_FAILURE)
    {
        retry();
        return;
    }
    retry();
    _port = port;
  }
  virtual ~syslog_conn()
  {
    if(_syslog_server)
    {
      RW_FREE(_syslog_server);
    }
    if (_socket_descriptor != -1) {
        close(_socket_descriptor);
        _socket_descriptor = -1;
    }
  }

  /* This is syslog specific variant of filling common attributes */
  virtual int fill_common_attributes(ProtobufCMessage *arrivingmsg,
                                     rwlog_hdr_t *log_hdr,
                                     std::ostringstream &os,
                                     rwlog_msg_id_key_t  *msg_id=0)
  {
    common_params_t *common_params = (common_params_t *)((uint8_t *)arrivingmsg+log_hdr->common_params_offset);
    time_t evtime = common_params->sec;
    struct tm* tm = gmtime(&evtime);
    char tmstr[128];
    int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
    sprintf(tmstr+tmstroff, ".%03uZ", common_params->usec/1000);

    /* Below is RFC 5424 syslog protocol format we comply to 
     * <PRI>V SP DATE SP HOST SP APPNAME  SP PROCESSID SP MSGID SP  STRUCTDATA SP MSG
     * where below fields are filled in each.
     * APPNAME - Can be set to Unique Riftware instance name/id and can help do filtering in Syslog conf based on this. 
     *            Currently set to configured syslog sink name. 
     * PROCESSID - PID-TID
     * MSGID - '-'
     * STRUCTDATA - '-'
     * MSG - FACILITY-NAME SP FILE:LINENO SP LOG_CATEGORY SP LOG_NOTIFICATION_NAME SP EVENT-ID SP ACTUALMSG
     *
     * MSGID/STRUCTDATA don't hold any of info from MSG to be backward compatible with RFC 3164 and avoid loosing these information.
     * */

    /*PRI & VERSION */
    uint32_t pri = LOG_USER + log_hdr->log_severity;
    os << "<" << pri << ">" << "1 ";
    /* Timestamp */
    os << tmstr;
    /* Hostname */
    os << " " << common_params->hostname;
    /* Appname */
    if(!uuid_is_null(log_hdr->vnf_id)) {
      char vnf_id_str[36+1];
      uuid_unparse(log_hdr->vnf_id,vnf_id_str);
      os << " " << vnf_id_str;
    }else {
      os << " " << sink_name_;
    }
    /* Processid */
    os << " " << common_params->processid << "-" << common_params->threadid;
    /* Msgid & Structed Data */
    os << " " << "-";
    os << " " << "-" << " ";

    /* Msg */
    os << common_params->appname << " ";
    std::string pathname = common_params->filename;
    os << pathname.substr(pathname.find_last_of("/\\") + 1) << ":" << common_params->linenumber << " ";

    const ProtobufCMessageDescriptor *log_desc = arrivingmsg->descriptor;
    if (log_desc && log_desc->ypbc_mdesc && log_desc->ypbc_mdesc->module) {
      os << log_desc->ypbc_mdesc->module->module_name;
      os << " " << log_desc->ypbc_mdesc->yang_node_name;
    } else {
      os << "RIFTWARE ERROR: NULL LOG. PLEASE CHECK";
    }
    os << " " << common_params->event_id;

#if 0
    if(!uuid_is_null(log_hdr->vnf_id)) {
      char vnf_id_str[36+1];
      uuid_unparse(log_hdr->vnf_id,vnf_id_str);
      os << " vnf-id:" << vnf_id_str;
    }
#endif

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

  virtual int post_evlog(uint8_t *proto )
  {
    char syslog[SYSLOG_MAX_SZ];
    int n = 0;
    //memset(syslog,'\0', SYSLOG_MAX_SZ);
    if(convert_log_to_str(proto,syslog, SYSLOG_MAX_SZ) > 0)
    {
      struct sockaddr_in servaddr;
      log_filter_matched++;

      bzero(&servaddr,sizeof(servaddr));
      servaddr.sin_family = AF_INET;
      servaddr.sin_addr = _server_ip;
      servaddr.sin_port = htons(_port);
      n = sendto(_socket_descriptor,syslog,strlen(syslog), 0,
                 (struct sockaddr *)&servaddr,sizeof(servaddr));
      //TEST: printf ("Sent Bytes %d error:%s\n", n, strerror(errno));
    }
    return n;
  }
  void poll()
  {
  }
private:
  rw_status_t set_server_ip(const char * hostname)
  {
    struct hostent *host_entry;
    struct in_addr **address_list;
    int i;

    if ((host_entry = gethostbyname(hostname)) == NULL)
    {
        herror("gethostbyname");
        _syslog_server = RW_STRDUP(hostname);
        //RWLOG_ASSERT(0);
        retry();
        return RW_STATUS_FAILURE;
    }

    address_list = (struct in_addr **) host_entry->h_addr_list;

    for(i = 0; address_list[i] != NULL; i++)
    {
        set_server_ip(address_list[i]);
        return RW_STATUS_SUCCESS;
    }
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  void set_server_ip (struct in_addr *ip_address)
  {
    set_state (RESOLVED);
    _server_ip = *ip_address;
    RWLOG_TRACE ("Syslog using IP %s \n", 
                 inet_ntoa(*ip_address));
  }
  void retry ()
  {
  }
  rw_status_t connect()
  {
    _socket_descriptor = socket(AF_INET,SOCK_DGRAM,0);
    set_state(CONNECTED);
    return RW_STATUS_SUCCESS;
  }
  virtual void rwlog_dump_info(int verbose)
  {
    printf ("Server Name %s ip %s port %d \n", (_syslog_server)? _syslog_server: "unresolved" ,inet_ntoa(_server_ip), _port);
    printf ("SD = %d \n", _socket_descriptor);
    rwlogd_rt_sink_conn::rwlog_dump_info(verbose);
  }
}; 
/*****************************************
 * APis to add the Sinks to the Rwlogd 
 *****************************************/
extern "C" rw_status_t start_rwlogd_syslog_sink(rwlogd_instance_ptr_t inst, 
                                                 const char *name, 
                                                 const char *host, int port)
{
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(name);    
  if (sink) 
  {   
    RWLOG_FILTER_DEBUG_PRINT("SINK MATCHED****** %s\n", name);
    return RW_STATUS_DUPLICATE;
  }   
  syslog_conn *conn = new syslog_conn(name , host, port);
  obj->add_sink (conn);
  return RW_STATUS_SUCCESS;
}
extern "C" rw_status_t stop_rwlogd_syslog_sink(rwlogd_instance_ptr_t inst, 
                                                 const char *name, 
                                                 const char *host, int port)
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
