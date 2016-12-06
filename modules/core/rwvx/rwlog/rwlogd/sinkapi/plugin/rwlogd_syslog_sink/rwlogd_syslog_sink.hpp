
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
