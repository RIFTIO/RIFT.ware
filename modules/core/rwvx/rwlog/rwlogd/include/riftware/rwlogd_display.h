
/*
 * STANDARD_RIFT_IO_COPYRIGHT
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
