
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

/* Probably futile attempt to avoid lotsa yang linkage in all rwmsg runtime consumers */

#ifndef __RWMSG_BROKER_MGMT_H__
#define __RWMSG_BROKER_MGMT_H__

__BEGIN_DECLS


rw_status_t rwmsg_broker_mgmt_get_chtab_FREEME(rwmsg_broker_t *bro, int *ct_out, RWPB_T_MSG(RwmsgData_Channelinfo) **chtab_freeme);
void rwmsg_broker_mgmt_get_chtab_free(int ct, RWPB_T_MSG(RwmsgData_Channelinfo) *chtab);

rw_status_t rwmsg_broker_mgmt_get_chtab_debug_info_FREEME(rwmsg_broker_t *bro, int *ct_out, RWPB_T_MSG(RwmsgData_ChannelDebugInfo) **chtab_freeme);
void rwmsg_broker_mgmt_get_chtab_debug_info_free(int ct, RWPB_T_MSG(RwmsgData_ChannelDebugInfo) *chtab);
__END_DECLS

#endif // __RWMSG_BROKER_MGMT_H__
