
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __RWLOGD_SYSLOG_SINK_HPP__
#define __RWLOGD_SYSLOG_SINK_HPP__

#include<stdio.h> 
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<errno.h>
#include<netdb.h> 
#include<arpa/inet.h>
#define IP_LEN 46
#include "rw-log.pb-c.h"

#include "rwlogd_sink_common.hpp"
#include "rwlogd_common.h"
#define SYSLOG_MAX_SZ 4096
#define RWLOGD_CLI_SINK_REGISTER (_sink_sufix) \
    do { \
      sink_conn = rwlogd_create_sink_ ## _sink_sufix(); \
      TAILQ_INSERT_TAIL(rwlogd_instance_data->sink_list, sink_conn); \
    } while (0)
#endif
