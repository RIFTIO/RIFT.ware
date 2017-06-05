/* STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file rwdts_api_gi_enum.h
 * @brief Introspectable enums for RW.DTS
 * @author Rajesh Velandy (rajesh.velandy@riftio.com)
 * @date 2015/04/16
 *
 */
#define  RWDTS_DEFINE_PROTO_ENUMS 1

#ifndef __RWDTS_API_GI_ENUM_H__
#define __RWDTS_API_GI_ENUM_H__

#include <glib.h>
#include <glib-object.h>
#include <rw_keyspec_gi.h>
#include <rwsched_gi.h>
#include <rw_pb.h>
#ifndef __GI_SCANNER__
#include <rw-dts.pb-c.h>
#endif

__BEGIN_DECLS
#ifdef RWDTS_DEFINE_PROTO_ENUMS
/*!
 * DTS Member state
 *
 */
#ifdef __GI_SCANNER__
typedef enum { rwdts_state_t_garbage } rwdts_state_t;
#else
typedef RWPB_E(RwDts_State) rwdts_state_t;
#endif

GType rwdts_state_get_type(void);

/*!
 *
 * DTS Query action.
 * ATTN - The following is a duplicate defintion of RWDtsQueryAction in rwdts.proto
 * This need to go away once there is GI bindings available for  .proto files.
 *
 */
#ifdef __GI_SCANNER__
typedef enum _RWDtsQueryAction {
  RWDTS_QUERY_INVALID = 0,
  RWDTS_QUERY_CREATE  = 1,
  RWDTS_QUERY_READ    = 2,
  RWDTS_QUERY_UPDATE  = 3,
  RWDTS_QUERY_DELETE  = 4,
  RWDTS_QUERY_RPC     = 5
} RWDtsQueryAction;


typedef enum _RWDtsXactMainState {
  RWDTS_XACT_INVALID   = 0,
  RWDTS_XACT_INIT      = 1,
  RWDTS_XACT_RUNNING   = 2,
  RWDTS_XACT_COMMITTED = 3,
  RWDTS_XACT_ABORTED   = 4,
  RWDTS_XACT_FAILURE   = 5
} RWDtsXactMainState;


typedef enum _RWDtsRtrConnState {
  RWDTS_RTR_STATE_DOWN = 0,
  RWDTS_RTR_STATE_UP   = 1
} RWDtsRtrConnState;

#endif /* __GI_SCANNER__ */

typedef enum _RWDtsXactFlag {

  RWDTS_XACT_FLAG_NONE           = 0,


  /* See also strings in both rwdts_router_xact.c and rwdts_api.c Gi binding glue */

  /* RWDtsXactFlags.  Many of these shouldn't be here at all. */
  RWDTS_XACT_FLAG_NOTRAN         = (1<<0),
  RWDTS_XACT_FLAG_TRANS_CRUD     = (1<<1), /* This is used for internal DTS operation */
  RWDTS_XACT_FLAG_UPDATE_CREDITS = (1<<2),
  RWDTS_XACT_FLAG_END            = (1<<3),
  RWDTS_XACT_FLAG_EXECNOW        = (1<<4),
  RWDTS_XACT_FLAG_WAIT           = (1<<5),
  RWDTS_XACT_FLAG_IMM_BOUNCE     = (1<<6),
  RWDTS_XACT_FLAG_REG            = (1<<7),
  RWDTS_XACT_FLAG_PEER_REG       = (1<<8),
  RWDTS_XACT_FLAG_BLOCK_MERGE    = (1<<9),
  RWDTS_XACT_FLAG_STREAM         = (1<<10),
  RWDTS_XACT_FLAG_TRACE          = (1<<11),
  RWDTS_XACT_FLAG_SOLICIT_RSP    = (1<<12),
  RWDTS_XACT_FLAG_ADVISE         = (1<<13),
  RWDTS_XACT_FLAG_DEPTH_FULL     = (1<<14),
  RWDTS_XACT_FLAG_DEPTH_OBJECT   = (1<<15),
  RWDTS_XACT_FLAG_DEPTH_LISTS    = (1<<16),
  RWDTS_XACT_FLAG_DEPTH_ONE      = (1<<17),
  RWDTS_XACT_FLAG_RETURN_PAYLOAD = (1<<18),
  RWDTS_XACT_FLAG_ANYCAST        = (1<<19),
  RWDTS_XACT_FLAG_REPLACE        = (1<<20),
  RWDTS_XACT_FLAG_SUB_READ       = (1<<21),
  RWDTS_XACT_FLAG_KEYLESS        = (1<<22)

} RWDtsXactFlag;  
  

typedef enum _RWDtsFlag {

  RWDTS_FLAG_NONE                = 0,


  /* See also strings in both rwdts_router_xact.c and rwdts_api.c Gi binding glue */

  /* RWDtsFlags */
  RWDTS_FLAG_INTERNAL_REG   = (1<<0), /* This is used for internal DTS operation */
  RWDTS_FLAG_DELTA_READY    = (1<<1),
  RWDTS_FLAG_SUBOBJECT      = (1<<2),
  RWDTS_FLAG_DEPTH_FULL     = (1<<3),
  RWDTS_FLAG_DEPTH_OBJECT   = (1<<4),
  RWDTS_FLAG_DEPTH_LISTS    = (1<<5),
  RWDTS_FLAG_DEPTH_ONE      = (1<<6),
  RWDTS_FLAG_NO_PREP_READ   = (1<<7),
  RWDTS_FLAG_SUBSCRIBER     = (1<<8),
  RWDTS_FLAG_PUBLISHER      = (1<<9),
  RWDTS_FLAG_DATASTORE      = (1<<10),
  RWDTS_FLAG_CACHE          = (1<<11),
  RWDTS_FLAG_SHARED         = (1<<12),
  RWDTS_FLAG_SHARDING       = (1<<13),
  RWDTS_FLAG_REGISTRATION_BOUNCE_OK = (1 << 14), //This flag is used by the router to ignore the bounce from the member 
} RWDtsFlag;

GType rwdts_query_action_get_type(void);
GType rwdts_flag_get_type(void);
GType rwdts_xact_flag_get_type(void);
GType rwdts_xact_main_state_get_type(void);


/*!
 *  DTS member response codes for the DTS transaction callbacks
 */

enum rwdts_member_rsp_code_e {
  RWDTS_ACTION_OK = 1729,
  RWDTS_ACTION_NA = 1730,
  RWDTS_ACTION_ASYNC = 1731,
  RWDTS_ACTION_NOT_OK = 1732,
  RWDTS_ACTION_INTERNAL = 1733
};
typedef enum rwdts_member_rsp_code_e rwdts_member_rsp_code_t;
GType rwdts_member_rsp_code_get_type(void);

/*!
 A code provided with a query callback indicating which event the
 callback is signalling.  Query callbacks themselves provide little
 information beyond which flavor of event has occured; the application
 can then obtain the desired result data or transaction status detail
 with assorted transaction, block, or query APIs.
*/
typedef enum rwdts_event_e {
  RWDTS_EVENT_STATUS_INVALID = 0,
  RWDTS_EVENT_STATUS_CHANGE,   /*!< There is a status change for this query or transaction. */
  RWDTS_EVENT_RESULT_READY     /*!< There are results available for this query. */
} rwdts_event_t;

GType rwdts_event_get_type(void);


/*!
 * DTS member event
 */
enum rwdts_member_event_e {
  RWDTS_MEMBER_EVENT_INVALID = 0,
  RWDTS_MEMBER_EVENT_base = 0x5520,
  RWDTS_MEMBER_EVENT_PREPARE,
  RWDTS_MEMBER_EVENT_CHILD,
  RWDTS_MEMBER_EVENT_PRECOMMIT,
  RWDTS_MEMBER_EVENT_COMMIT,
  RWDTS_MEMBER_EVENT_ABORT,
  RWDTS_MEMBER_EVENT_AUDIT,
  RWDTS_MEMBER_EVENT_INSTALL,
  RWDTS_MEMBER_EVENT_end,

  RWDTS_MEMBER_EVENT_FIRST = RWDTS_MEMBER_EVENT_base+1,
  RWDTS_MEMBER_EVENT_LAST = RWDTS_MEMBER_EVENT_end-1,
  RWDTS_MEMBER_EVENT_COUNT = RWDTS_MEMBER_EVENT_end - RWDTS_MEMBER_EVENT_base + 1
};

/*!
 * Get the member event as an index
 */
#define RWDTS_MEMBER_EVENT_INDEX(evt) ((evt) - RWDTS_MEMBER_EVENT_base - 1)

typedef enum rwdts_member_event_e rwdts_member_event_t;

GType rwdts_member_event_get_type(void);

/*!
 * Group API phase.
 *
 */
enum rwdts_group_phase_e {
  RWDTS_GROUP_PHASE_INVALID=0,
  RWDTS_GROUP_PHASE_REGISTER=1
};

typedef enum rwdts_group_phase_e rwdts_group_phase_t;

GType rwdts_group_phase_get_type(void);

#else /*  RWDTS_DEFINE_PROTO_ENUMS */
/*!
 * DTS Member state
 *
 */
typedef enum rwdts_state_e {
  RW_DTS_STATE_NULL   = 0,
  RW_DTS_STATE_INIT          = 1, // DTS-READY
  RW_DTS_STATE_REGN_COMPLETE = 2, // APP_REGISTRATIONS_COMPLETE
  RW_DTS_STATE_CONFIG_READY  = 3, // DTS_DATA_RECOVERY_COMPLETE
  RW_DTS_STATE_CONFIG_DONE   = 4  // APP_RECOVERY_COMPLETE
} rwdts_state_t;

GType rwdts_state_get_type(void);

/*!
 *  DTS member response codes for the DTS transaction callbacks
 */

enum rwdts_member_rsp_code_e {
  RWDTS_ACTION_OK = 1729,     /*!< Member is okay with this transaction */
  RWDTS_ACTION_NA = 1730,     /*!< Member is does not care about this transaction */
  RWDTS_ACTION_ASYNC = 1731,  /*!< Member will respond to this transaction asynchronously using rwdts_member_send_response() */
  RWDTS_ACTION_NOT_OK = 1732  /*!< Member is not okay with this transaction */
};
typedef enum rwdts_member_rsp_code_e rwdts_member_rsp_code_t;

GType rwdts_member_rsp_code_get_type(void);

/*!
 A code provided with a query callback indicating which event the
 callback is signalling.  Query callbacks themselves provide little
 information beyond which flavor of event has occured; the application
 can then obtain the desired result data or transaction status detail
 with assorted transaction, block, or query APIs.
*/
typedef enum rwdts_event_e {
  RWDTS_EVENT_STATUS_INVALID = 0,
  RWDTS_EVENT_STATUS_CHANGE,   /*!< There is a status change for this query or transaction. */
  RWDTS_EVENT_RESULT_READY     /*!< There are results available for this query. */
} rwdts_event_t;

GType rwdts_event_get_type(void);

/*!
 * DTS member event
 */
enum rwdts_member_event_e {
  RWDTS_MEMBER_EVENT_INVALID = 0,
  RWDTS_MEMBER_EVENT_base = 0x5520,
  RWDTS_MEMBER_EVENT_PREPARE,
  RWDTS_MEMBER_EVENT_CHILD,
  RWDTS_MEMBER_EVENT_PRECOMMIT,
  RWDTS_MEMBER_EVENT_COMMIT,
  RWDTS_MEMBER_EVENT_ABORT,
  RWDTS_MEMBER_EVENT_INSTALL,
  RWDTS_MEMBER_EVENT_end,

  RWDTS_MEMBER_EVENT_FIRST = RWDTS_MEMBER_EVENT_base+1,
  RWDTS_MEMBER_EVENT_LAST = RWDTS_MEMBER_EVENT_end-1,
  RWDTS_MEMBER_EVENT_COUNT = RWDTS_MEMBER_EVENT_end - RWDTS_MEMBER_EVENT_base + 1
};
GType rwdts_member_event_get_type(void);

/*!
 * Group API phase.
 *
 */
enum rwdts_group_phase_e {
  RWDTS_GROUP_PHASE_INVALID=0,
  RWDTS_GROUP_PHASE_REGISTER=1
};

typedef enum rwdts_group_phase_e rwdts_group_phase_t;

GType rwdts_group_phase_get_type(void);

/*!
 * Get the member event as an index
 */
#define RWDTS_MEMBER_EVENT_INDEX(evt) ((evt) - RWDTS_MEMBER_EVENT_base - 1)

typedef enum rwdts_member_event_e rwdts_member_event_t;
#endif /* RWDTS_DEFINE_PROTO_ENUMS */

typedef enum rwdts_xact_rsp_code_e {
  RWDTS_XACT_RSP_CODE_ACK = 1001,
  RWDTS_XACT_RSP_CODE_NACK,
  RWDTS_XACT_RSP_CODE_NA,
  RWDTS_XACT_RSP_CODE_MORE
} rwdts_xact_rsp_code_t;

GType rwdts_xact_rsp_code_get_type(void);

/*!
 * App conf phase
 */
enum rwdts_appconf_phase_e {
  RWDTS_APPCONF_PHASE_INVALID=0,
  RWDTS_APPCONF_PHASE_REGISTER=1
};

typedef enum rwdts_appconf_phase_e rwdts_appconf_phase_t;

GType rwdts_appconf_phase_get_type(void);

/*!
 * DTS appconf action passed in config apply
 */
typedef enum {
  RWDTS_APPCONF_ACTION_INSTALL=1,
  RWDTS_APPCONF_ACTION_RECONCILE,
  RWDTS_APPCONF_ACTION_AUDIT,
  RWDTS_APPCONF_ACTION_RECOVER
} rwdts_appconf_action_t;

GType rwdts_appconf_action_get_type(void);

/**
 * rwdts_action_str:
 * @action:  DTS action
 */
const char *
rwdts_action_str(rwdts_appconf_action_t action);

typedef enum rwdts_audits_status_e {
  RWDTS_AUDIT_STATUS_NULL = 0,
  RWDTS_AUDIT_STATUS_SUCCESS = 1,
  RWDTS_AUDIT_STATUS_FAILURE = 2,
  RWDTS_AUDIT_STATUS_ABORTED = 3
} rwdts_audit_status_t;
GType rwdts_audit_status_get_type(void);

typedef enum rwdts_audit_action_e {
  RWDTS_AUDIT_ACTION_REPORT_ONLY = 0,
  RWDTS_AUDIT_ACTION_RECONCILE = 1,
  RWDTS_AUDIT_ACTION_RECOVER = 2
} rwdts_audit_action_t;
GType rwdts_audit_action_get_type(void);


typedef enum RwDts__YangEnum__ShardFlavor__E rwdts_shard_flavor;
//typedef enum RWPB_E(RwDts_SharedFlavor) rwdts_shard_flavor;


GType rwdts_shard_flavor_get_type(void);

typedef enum rwdts_shard_keyfunc_e {
  RWDTS_SHARD_KEYFUNC_NOP,
  RWDTS_SHARD_KEYFUNC_BLOCKS,
  RWDTS_SHARD_KEYFUNC_HASH1,
} rwdts_shard_keyfunc_t;
GType rwdts_shard_keyfunc_get_type(void);

typedef enum rwdts_shard_anycast_policy_e {
  RWDTS_SHARD_DEMUX_WEIGHTED,
  RWDTS_SHARD_DEMUX_ORDERED,
  RWDTS_SHARD_DEMUX_ROUND_ROBIN,
} rwdts_shard_anycast_policy_t; 
GType rwdts_shard_anycast_get_type(void);

__END_DECLS

#endif /* __RWDTS_API_GI_ENUM_H__ */
