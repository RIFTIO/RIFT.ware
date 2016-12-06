
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

#ifndef __RWLOGD_SHOW_H__
#define __RWLOGD_SHOW_H__

#include "rwtasklet.h"
#include "rwlogd.pb-c.h"
#include "rwlog-mgmt.pb-c.h"
#include "rw-log.pb-c.h"
#include "rwdts.h"


#ifdef __cplusplus
extern "C" {
#endif

#define CLI_DEFAULT_LOG_LINES 100

rw_status_t
rwlogd_fetch_logs_from_other_rwlogd (const rwdts_xact_info_t* xact_info,
                                 RWDtsQueryAction         action,
                                 const rw_keyspec_path_t*      key,
                                 const ProtobufCMessage*  msg);

#ifdef __cplusplus
}
#endif

#endif //__RWLOGD_SHOW_H__
