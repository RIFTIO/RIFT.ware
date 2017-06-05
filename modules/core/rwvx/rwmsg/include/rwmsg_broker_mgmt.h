
/*
 * STANDARD_RIFT_IO_COPYRIGHT
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
