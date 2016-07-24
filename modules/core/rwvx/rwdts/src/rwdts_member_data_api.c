
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwdts_member_data_api.c
 * @author Rajesh Velandy
 * @date 2014/09/15
 * @brief DTS  member data API definitions
 */

#include <stdlib.h>
#include <time.h>
#include <rwtypes.h>
#include <rwdts_int.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_keyspec.h>
#include <rwdts.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <rwlib.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>

#define RWDTS_DATA_MEMBER_CONST_KEY (const uint8_t*)"Hmmm...."
#define RWDTS_MAX_MINIKEY_LEN 128

/*
 * The following macro forces the keyspec to category to that of the registration
 * if the input ks category is ANY
 */

#define RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(__apih__, __in_ks__, __reg_ks__) \
do { \
  if (rw_keyspec_path_get_category(__in_ks__) == RW_SCHEMA_CATEGORY_ANY) { \
    rw_keyspec_path_set_category((__in_ks__), &((__apih__)->ksi), rw_keyspec_path_get_category(__reg_ks__)); \
    rw_keyspec_path_delete_binpath((__in_ks__), &((__apih__)->ksi)); \
  } \
  RW_ASSERT(rw_keyspec_path_get_category(__in_ks__) != RW_SCHEMA_CATEGORY_ANY); \
  RW_ASSERT(rw_keyspec_path_get_category(__in_ks__) == rw_keyspec_path_get_category(__reg_ks__)); \
} while (0)

/*
 * Internal structure to send data between data member APIs for async dispatch
 */

typedef struct rwdts_member_data_record_s {
  rwdts_member_registration_t*        reg;
  rwdts_xact_t*                       xact;
  rw_keyspec_entry_t*                 pe;
  rw_schema_minikey_opaque_t*         mk;
  ProtobufCMessage*                   msg;
  uint32_t                            flags;
  rw_keyspec_path_t*                  keyspec;
  RwDts__YangEnum__AuditTrailEvent__E updated_by;
  rwdts_event_cb_t                    member_advise_cb;
  rwdts_event_cb_t                    rcvd_member_advise_cb;
} rwdts_member_data_record_t;

typedef enum {
  RWDTS_CREATE = 1,
  RWDTS_UPDATE = 2,
  RWDTS_DELETE = 3
} rwdts_member_data_action_t;



/*******************************************************************************
 *                      Static prototypes                                      *
 *******************************************************************************/

static rw_status_t
rwdts_member_data_create_list_element_f(rwdts_member_data_record_t *mbr_data);

static rw_status_t
rwdts_member_data_update_list_element_f(rwdts_member_data_record_t *mbr_data);

static rw_status_t
rwdts_member_data_delete_list_element_f(rwdts_member_data_record_t *mbr_data);

static rw_status_t
rwdts_member_data_create_list_element_w_key_f(rwdts_member_data_record_t *mbr_data);

static rw_status_t
rwdts_member_data_create_list_element_w_key(rwdts_xact_t*             xact,
                                            rwdts_member_reg_handle_t regh,
                                            rw_keyspec_path_t*             keyspec,
                                            const ProtobufCMessage*   msg);
static rw_status_t
rwdts_member_data_update_list_element_w_key_f(rwdts_member_data_record_t *mbr_data);

static rw_status_t
rwdts_member_data_delete_list_element_w_key_f(rwdts_member_data_record_t *mbr_data);


static rwdts_member_data_record_t*
rwdts_member_data_record_init(rwdts_xact_t*                xact,
                              rwdts_member_registration_t* reg,
                              rw_keyspec_entry_t*     pe,
                              rw_schema_minikey_opaque_t*  mk,
                              const ProtobufCMessage*      msg,
                              uint32_t                     flags,
                              rw_keyspec_path_t*           keyspec,
                              rwdts_event_cb_t*            member_advise_cb);

static
void rwdts_member_data_merge_store(rwdts_member_data_record_t *mbr_data,
                                   rwdts_member_data_object_t*  mobj);

static void
rwdts_member_pub_del(rwdts_member_data_record_t *mbr_data,
                     rwdts_member_data_object_t* mobj);

static
rwdts_member_data_object_t*
rwdts_member_data_store(rwdts_member_data_object_t** obj_list_p,
                        rwdts_member_data_record_t *mbr_data,
                        uint8_t *free_obj);

static rw_status_t
rwdts_member_data_record_deinit(rwdts_member_data_record_t *mbr_data);

static inline void
rwdts_expand_member_data_record(rwdts_member_data_record_t*   mbr_data,
                                rwdts_xact_t**                xact_p,
                                rwdts_member_registration_t** reg_p,
                                rw_keyspec_entry_t**          pe_p,
                                rw_schema_minikey_opaque_t**  mk,
                                ProtobufCMessage**            msg_p,
                                uint32_t*                     flags,
                                rw_keyspec_path_t**           keyspec);



static rw_status_t
rwdts_member_data_create_list_element(rwdts_xact_t*             xact,
                                      rwdts_member_reg_handle_t regh,
                                      rw_keyspec_entry_t*       pe,
                                      const ProtobufCMessage*   msg);

static void 
rwdts_member_data_finalize(rwdts_member_data_record_t*    mbr_data,
                           rwdts_member_data_object_t*    mobj,
                           RWDtsQueryAction               query_action,
                           RWPB_E(RwDts_AuditTrailAction) audit_action);

rw_status_t
rwdts_member_reg_handle_handle_create_update_element(rwdts_member_reg_handle_t  regh,
                                                     rw_keyspec_path_t*         keyspec,
                                                     const ProtobufCMessage*    msg,
                                                     rwdts_xact_t*              xact,
                                                     uint32_t                   flags,
                                                     rwdts_member_data_action_t action);

/*******************************************************************************
 *                      Static functions                                       *
 *******************************************************************************/

static rwdts_kv_light_reply_status_t
rwdts_member_data_cache_set_callback(rwdts_kv_light_set_status_t status,
                                     int serial_num, void *userdata)
{
  //rwdts_kv_handle_t *handle = (rwdts_kv_handle_t *)userdata;

  if (RWDTS_KV_LIGHT_SET_SUCCESS == status) {
    /* DO Nothing */
  }

  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static void 
rwdts_member_data_finalize(rwdts_member_data_record_t*    mbr_data,
                           rwdts_member_data_object_t*    mobj,
                           RWDtsQueryAction               query_action,
                           RWPB_E(RwDts_AuditTrailAction) audit_action)
{
  RW_ASSERT(mbr_data);
  RW_ASSERT(mobj);

  if ((mbr_data->reg->flags & RWDTS_FLAG_DATASTORE) != 0) {
    if (mbr_data->xact != NULL) {
      rwdts_kv_update_db_xact_precommit(mobj, query_action);
    } else {
      rwdts_kv_update_db_update(mobj, query_action);
    }
  }

  /* Add an audit trail that this record is updated */
  if (RWDTS_QUERY_DELETE != query_action) {
    rwdts_member_data_object_add_audit_trail(mobj,
                                             mbr_data->updated_by,
                                             audit_action);
  }

  if ((mbr_data->reg->flags & RWDTS_FLAG_PUBLISHER) != 0) {
    uint32_t flags = 0;
    if (mbr_data->member_advise_cb.cb)  {
      flags |= RWDTS_XACT_FLAG_EXECNOW;
    }
    rwdts_member_data_advice_query_action(mbr_data->xact,
                                          (rwdts_member_reg_handle_t)mobj->reg,
                                          mobj->keyspec,
                                          mobj->msg,
                                          query_action,
                                          flags,
                                          &mbr_data->member_advise_cb,
                                          &mbr_data->rcvd_member_advise_cb,
                                          true);

    if (!mbr_data->xact && (mbr_data->reg->apih->rwtasklet_info && 
        ((mbr_data->reg->apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_BDB) ||
        (mbr_data->reg->apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_REDIS)))) {
      if (mbr_data->reg->kv_handle) {
        uint8_t *payload;
        size_t  payload_len;
        payload = protobuf_c_message_serialize(NULL, mobj->msg, &payload_len);
        rw_status_t rs = rwdts_kv_handle_add_keyval(mbr_data->reg->kv_handle, 0, (char *)mobj->key, mobj->key_len,
                                                   (char *)payload, (int)payload_len, rwdts_member_data_cache_set_callback, 
                                                   (void *)mbr_data->reg->kv_handle);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
        if (rs != RW_STATUS_SUCCESS) {
          /* Fail the VM and switchover to STANDBY */
        }
      }
    }
  }
  return;
}

/*
 * Call back for member data advises
 */
void
rwdts_member_data_advise_cb(rwdts_xact_t*        xact,
                            rwdts_xact_status_t* xact_status,
                            void*                ud)
{

  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_memer_data_adv_event_t* member_advise_cb;
  rwdts_api_t *apih = xact->apih;
  rwdts_member_registration_t *reg;

  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT(xact_status);

  member_advise_cb =  (rwdts_memer_data_adv_event_t*)ud;
  RW_ASSERT_TYPE (member_advise_cb, rwdts_memer_data_adv_event_t);
  if (member_advise_cb == NULL) {
    printf("%s:%d -- member_advise_cb() is NULL\n", __FILE__, __LINE__);
    return;
  }

  // ATTN to make rwdts_pytest.py happy - Use reg directly once it is properly reference
  // counted.
  RW_SKLIST_LOOKUP_BY_KEY(&(apih->reg_list), &member_advise_cb->reg_id, (void *)&reg);
  
  if (reg) {
    RW_ASSERT(reg == member_advise_cb->reg);
    RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  } else {
    printf("%s:%d -- registration not found %u\n", __FILE__, __LINE__, member_advise_cb->reg_id);
  }


  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, DATA_MEMBER_API_CB, "Received callback for data member API triggered advise", xact_status->status);

  RW_ASSERT_TYPE(xact->apih, rwdts_api_t);


  switch (xact_status->status) {
    case RWDTS_XACT_INIT:
    case RWDTS_XACT_RUNNING:
      break;
    case RWDTS_XACT_ABORTED:
      if (!member_advise_cb->solicit_triggered) {
        xact->apih->stats.num_member_advise_aborted++;
      }
      RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, DATA_MEMBER_API_ERROR, "Received XACT-STATUS-ABORT in callback for data member API triggered advise");
      break;
    case RWDTS_XACT_FAILURE:
      if (!member_advise_cb->solicit_triggered) {
        xact->apih->stats.num_member_advise_failed++;
      }
      RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, DATA_MEMBER_API_ERROR, "Received XACT-STATUS-FAILURE in callback for data member API triggered advise");
      break;
    default:
      if (!member_advise_cb->solicit_triggered) {
        xact->apih->stats.num_member_advise_success++;
      }
      break;
  }
  if (reg && !member_advise_cb->block_event &&  xact_status->xact_done) {
#if 0
    //RIFT-11566 commit serial manage leaks if one out of order..
    //masking this for now
    rwdts_pub_serial_t serial;

    serial.id.member_id = reg->serial.id.member_id;
    serial.id.router_id = reg->serial.id.router_id;
    serial.id.pub_id    = reg->serial.id.pub_id;

    serial.serial = member_advise_cb->serial;

    rwdts_add_entry_to_commit_serial_list(&reg->committed_serials,
                                          &serial,
                                          &reg->highest_commit_serial);
#endif

  }
  if (member_advise_cb && member_advise_cb->event_cb.cb) {
    RW_ASSERT(rwdts_member_data_advise_cb != member_advise_cb->event_cb.cb);
    member_advise_cb->event_cb.cb(xact,
                                  xact_status,
                                  (void *)member_advise_cb->event_cb.ud);
  } 

  if (!member_advise_cb->solicit_triggered) {
    apih->stats.num_member_advise_rsp++;
  }
  if (xact_status->xact_done && member_advise_cb) {
    RW_FREE_TYPE (member_advise_cb, rwdts_memer_data_adv_event_t);
  }

  return;
}

rwdts_memer_data_adv_event_t*
rwdts_memer_data_adv_init(const rwdts_event_cb_t*      advise_cb,
                          rwdts_member_registration_t* reg,
                          bool                         solicit_triggered,
                          bool                         block_event)
{
  rwdts_memer_data_adv_event_t *member_advise_cb;
  member_advise_cb = RW_MALLOC0_TYPE(sizeof(rwdts_memer_data_adv_event_t),
                                     rwdts_memer_data_adv_event_t);
  RW_ASSERT_TYPE (member_advise_cb, rwdts_memer_data_adv_event_t);
  member_advise_cb->reg_id = reg->reg_id;
  member_advise_cb->reg = reg;
  
  RW_ASSERT(advise_cb->cb != rwdts_member_data_advise_cb);

  member_advise_cb->event_cb.cb = advise_cb->cb;
  member_advise_cb->event_cb.ud = advise_cb->ud;
  member_advise_cb->solicit_triggered = solicit_triggered;
  member_advise_cb->block_event = block_event;
  member_advise_cb->serial = reg->serial.serial+1; // ATTN - relook at this
  return member_advise_cb;
}


static rwdts_member_data_object_t*
rwdts_member_data_duplicate_obj(rwdts_member_data_object_t* mobj)
{
  rw_status_t rs;

  rwdts_member_data_object_t *newobj = RW_MALLOC0_TYPE(sizeof(rwdts_member_data_object_t),
                                                       rwdts_member_data_object_t);

  RW_ASSERT_TYPE(newobj, rwdts_member_data_object_t);

  newobj->msg =  protobuf_c_message_duplicate(NULL, mobj->msg, mobj->msg->descriptor);

  rs = RW_KEYSPEC_PATH_CREATE_DUP(mobj->keyspec, NULL , &newobj->keyspec, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(newobj->msg);
  RW_ASSERT(newobj->keyspec);


  // Get the keys from keyspec
  rs = RW_KEYSPEC_PATH_GET_BINPATH(newobj->keyspec, NULL , (const uint8_t **)&newobj->key, (size_t *)&newobj->key_len, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  newobj->reg = mobj->reg;

  return newobj;
}

/*******************************************************************************
 *                      public APIs                                            *
 *******************************************************************************/
/*
 * Init a data member object
 */

rwdts_member_data_object_t*
rwdts_member_data_init(rwdts_member_registration_t*        reg,
                       ProtobufCMessage*                   msg,
                       rw_keyspec_path_t*                  keyspec,
                       bool                                usemsg,
                       bool                                useks)
{
  rw_status_t rs;
  rwdts_api_t* apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_member_data_object_t *mobj = RW_MALLOC0_TYPE(sizeof(rwdts_member_data_object_t),
                                                     rwdts_member_data_object_t);

  RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);
  RW_ASSERT(keyspec);
  if (!keyspec) {
    return NULL;
  }

  if (usemsg) {
    mobj->msg = msg;
  } else {
    mobj->msg =  protobuf_c_message_duplicate(NULL, msg, msg->descriptor);

  }

  if (useks) {
    mobj->keyspec = keyspec;
  } else {
    rs = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &mobj->keyspec, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  RW_ASSERT(mobj->msg);
  RW_ASSERT(mobj->keyspec);

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, mobj->keyspec, reg->keyspec);

  if (protobuf_c_message_unknown_get_count(mobj->msg)) {
    rwdts_report_unknown_fields(mobj->keyspec, apih, mobj->msg);
    protobuf_c_message_delete_unknown_all(NULL, mobj->msg);
  }
  RW_ASSERT(!protobuf_c_message_unknown_get_count(mobj->msg))

  // Get the keys from keyspec
  rs = RW_KEYSPEC_PATH_GET_BINPATH(mobj->keyspec, NULL , (const uint8_t **)&mobj->key, (size_t *)&mobj->key_len, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  mobj->reg = reg;
  return mobj;
}

static rwdts_member_data_object_t*
rwdts_member_data_init_frm_mbr_data(rwdts_member_data_record_t *mbr_data,
                                    bool                        usemsg,
                                    bool                        useks,
                                    rwdts_member_data_action_t  action)
{
  rw_status_t rs;

  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);
  RW_ASSERT(mbr_data->keyspec);
  if (!mbr_data->keyspec) {
    return NULL;
  }
  RW_ASSERT_TYPE(mbr_data->reg, rwdts_member_registration_t);
  rwdts_api_t *apih = mbr_data->reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_member_data_object_t *mobj = RW_MALLOC0_TYPE(sizeof(rwdts_member_data_object_t),
                                                     rwdts_member_data_object_t);

  RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);

  if (action != RWDTS_DELETE) {
    RW_ASSERT(mbr_data->msg);
    if (usemsg) {
      mobj->msg = mbr_data->msg;
      mbr_data->msg = NULL; // Ownership transfered
    } else {
      mobj->msg =  protobuf_c_message_duplicate(NULL, mbr_data->msg,mbr_data->msg->descriptor);
    }
    RW_ASSERT(mobj->msg);
  }

  if (useks) {
    mobj->keyspec = mbr_data->keyspec;
    mbr_data->keyspec = NULL; // Ownership transfered
  } else {
    rs = RW_KEYSPEC_PATH_CREATE_DUP(mbr_data->keyspec, NULL , &mobj->keyspec, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, mobj->keyspec, mbr_data->reg->keyspec);

  // Get the keys from keyspec
  rs = RW_KEYSPEC_PATH_GET_BINPATH(mobj->keyspec, NULL , (const uint8_t **)&mobj->key, (size_t *)&mobj->key_len, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  mobj->reg = mbr_data->reg;

  return mobj;
}

static rwdts_member_data_object_t*
rwdts_member_dup_mobj(const rwdts_member_data_object_t *in_mobj)
{
  rw_status_t rs;
  RW_ASSERT_TYPE((rwdts_member_data_object_t*)in_mobj, rwdts_member_data_object_t);
  RW_ASSERT(in_mobj->keyspec);
  RW_ASSERT(in_mobj->msg);

  if (!in_mobj->keyspec) {
    return NULL;
  }
  if (!in_mobj->msg) {
    return NULL;
  }
  rwdts_member_registration_t *reg = in_mobj->reg;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_member_data_object_t *mobj = RW_MALLOC0_TYPE(sizeof(rwdts_member_data_object_t),
                                                     rwdts_member_data_object_t);

  RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);

  if (in_mobj->msg) {
    mobj->msg =  protobuf_c_message_duplicate(NULL, in_mobj->msg, in_mobj->msg->descriptor);
    RW_ASSERT(mobj->msg);
  }

  rs = RW_KEYSPEC_PATH_CREATE_DUP(in_mobj->keyspec, NULL , &mobj->keyspec, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS && mobj->keyspec != NULL);

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, mobj->keyspec, reg->keyspec);

  rs = RW_KEYSPEC_PATH_GET_BINPATH(mobj->keyspec, NULL , (const uint8_t **)&mobj->key, (size_t *)&mobj->key_len, NULL);

  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  mobj->reg = in_mobj->reg;

  return mobj;
}

/*
 * deinit a data member object
 */

rw_status_t
rwdts_member_data_deinit(rwdts_member_data_object_t *mobj)
{
  int i;

  RW_ASSERT(mobj);

  if (!mobj) {
    return RW_STATUS_FAILURE;
  }

  if (mobj->msg) {
    protobuf_c_message_free_unpacked(NULL, mobj->msg);
    mobj->msg = NULL;
  }

  if (mobj->keyspec) {
    RW_KEYSPEC_PATH_FREE(mobj->keyspec, NULL, NULL );
  }


  if (mobj->audit) {
    RW_FREE_TYPE(mobj->audit, rwdts_member_data_object_audit_t);
    mobj->audit = NULL;
  }

  /* Free all the audit entries */
  if (mobj->n_audit_trail > 0) {
    for (i = 0; i < mobj->n_audit_trail ; i++) {
      protobuf_c_message_free_unpacked(NULL, &mobj->audit_trail[i]->base);
      mobj->audit_trail[i] = NULL;
    }
    RW_FREE(mobj->audit_trail);
    mobj->audit_trail = NULL;
  } else {
    RW_ASSERT(NULL == mobj->audit_trail);
  }

  RW_FREE_TYPE(mobj, rwdts_member_data_object_t);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_member_data_object_add_audit_trail(rwdts_member_data_object_t*          mobj,
                                         RwDts__YangEnum__AuditTrailEvent__E  updated_by,
                                         RwDts__YangEnum__AuditTrailAction__E action)
{
  char   tmp_buf[64];
  time_t current_time;

  if (mobj->n_audit_trail >= RWDTS_MEMBER_OBJ_MAX_AUDIT_TRAIL) {
    // ATTN -- may be this should add this as the last entry
    return RW_STATUS_FAILURE;
  }
  mobj->audit_trail = (RWPB_T_MSG(RwDts_output_StartDts_AuditSummary_FailedReg_ObjDts_AuditTrail)**)
                      realloc(mobj->audit_trail, (mobj->n_audit_trail+1)* sizeof(RWPB_T_MSG(RwDts_output_StartDts_AuditSummary_FailedReg_ObjDts_AuditTrail)*));
  RW_ASSERT(mobj->audit_trail != NULL);

  int idx = mobj->n_audit_trail;

  mobj->n_audit_trail++;

  //mobj->audit_trail[idx] =  (RwDts__YangOutput__RwDts__StartDts__AuditSummary__FailedReg__ObjDts__AuditTrail*)
  //                          RW_MALLOC0(sizeof(RwDts__YangOutput__RwDts__StartDts__AuditSummary__FailedReg__ObjDts__AuditTrail));
  RWPB_M_MSG_DECL_PTR_ALLOC(RwDts_output_StartDts_AuditSummary_FailedReg_ObjDts_AuditTrail, tmpobj);
  mobj->audit_trail[idx] = tmpobj;
  mobj->audit_trail[idx]->updated_by = updated_by;
  mobj->audit_trail[idx]->has_updated_by = 1;
  mobj->audit_trail[idx]->action     = action;
  mobj->audit_trail[idx]->has_action = 1;
  time(&current_time);
  mobj->audit_trail[idx]->event_time = strdup(ctime_r(&current_time, tmp_buf));

  return (RW_STATUS_SUCCESS);
}

rwdts_xact_t*
rwdts_member_data_advice_query_action_xact(rwdts_xact_t*             xact,
                                           rwdts_xact_block_t*       block,
                                           rwdts_member_reg_handle_t regh,
                                           rw_keyspec_path_t*        keyspec,
                                           const ProtobufCMessage*   msg,
                                           RWDtsQueryAction          action,
                                           uint32_t                  flags,
                                           bool                      inc_serial)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  rwdts_api_t *apih = reg->apih;
  uint64_t corrid = 1000; /* Need to find out how to generate this */
  RW_ASSERT(apih);
  if (!apih) {
    return NULL;
  }
  rw_keyspec_path_t *merged_keyspec = NULL;
  rw_status_t rw_status;

  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(block);

  apih->stats.num_advises++;
  reg->stats.num_advises++;


  if (keyspec) {
    rw_status = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &merged_keyspec, NULL);
  } else {
    rw_status = RW_KEYSPEC_PATH_CREATE_DUP(reg->keyspec, NULL , &merged_keyspec, NULL);
  }
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_XACT_ABORT(xact, rw_status, RWDTS_ERRSTR_KEY_DUP);
    return NULL;
  }
  if (rw_keyspec_path_has_wildcards(merged_keyspec)) {
    RWDTS_XACT_ABORT(xact, rw_status, RWDTS_ERRSTR_KEY_WILDCARDS);
    RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL, NULL);
    return NULL;
  }

  RW_ASSERT(merged_keyspec);
  {
    char *tmp = NULL;

    rw_keyspec_path_get_new_print_buffer(merged_keyspec, NULL ,
                                rwdts_api_get_ypbc_schema(apih),
                                &tmp);

    RWDTS_API_LOG_EVENT(apih, KeyspecUpdatedPe, "Updated keyspec with path entry", tmp ? tmp : "");
    free(tmp);
  }

  flags |= RWDTS_XACT_FLAG_ADVISE;
  flags |= RWDTS_XACT_FLAG_NOTRAN;
  // Increment the serial associated with this registration 
  if (inc_serial) {
    rwdts_member_reg_handle_inc_serial(reg);
  }


  rw_status_t rs = rwdts_xact_block_add_query_ks_reg(block,
                              merged_keyspec,
                              action,
                              flags,
                              corrid,
                              msg,
                              reg);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  // Free the keyspec
  RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL , NULL);
  return xact;
}

rwdts_xact_t*
rwdts_member_data_advice_query_action(rwdts_xact_t*             xact,
                                      rwdts_member_reg_handle_t regh,
                                      rw_keyspec_path_t*        keyspec,
                                      const ProtobufCMessage*   msg,
                                      RWDtsQueryAction          action,
                                      uint32_t                  flags,
                                      rwdts_event_cb_t*         advise_cb,
                                      rwdts_event_cb_t*         rcvd_advise_cb,
                                      bool                      inc_serial)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  rwdts_api_t *apih = reg->apih;
  uint64_t corrid = 1000; /* Need to find out how to generate this */
  RW_ASSERT(apih);
  if (!apih) {
    return NULL;
  }
  rw_keyspec_path_t *merged_keyspec = NULL;
  rw_status_t rw_status;

  RW_ASSERT(action != RWDTS_QUERY_INVALID);

  apih->stats.num_advises++;
  reg->stats.num_advises++;

  if (keyspec) {
    rw_status = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &merged_keyspec, NULL);
  } else {
    rw_status = RW_KEYSPEC_PATH_CREATE_DUP(reg->keyspec, NULL , &merged_keyspec, NULL);
  }
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_XACT_ABORT(xact, rw_status, RWDTS_ERRSTR_KEY_DUP);
    return NULL;
  }
  if (rw_keyspec_path_has_wildcards(merged_keyspec)) {
    RWDTS_XACT_ABORT(xact, rw_status, RWDTS_ERRSTR_KEY_WILDCARDS);
    RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL, NULL);
    return NULL;
  }

  RW_ASSERT(merged_keyspec);
  {
    char *tmp = NULL;

    rw_keyspec_path_get_new_print_buffer(merged_keyspec, NULL ,
                                rwdts_api_get_ypbc_schema(apih),
                                &tmp);

    RWDTS_API_LOG_EVENT(apih, KeyspecUpdatedPe, "Updated keyspec with path entry", tmp ? tmp : "");
    free(tmp);
  }

  flags |= RWDTS_XACT_FLAG_ADVISE;

  // Increment the serial associated with this registration 
  if (inc_serial) {
    rwdts_member_reg_handle_inc_serial(reg);
  }

  if (!xact) {
//  flags |= RWDTS_XACT_FLAG_NOTRAN;
    flags |= RWDTS_XACT_FLAG_END;
    rwdts_memer_data_adv_event_t* member_adv_event = rwdts_memer_data_adv_init(rcvd_advise_cb,
                                                                               reg,
                                                                               false,
                                                                               false);
    RW_ASSERT_TYPE(member_adv_event, rwdts_memer_data_adv_event_t);
    advise_cb->ud = member_adv_event;
    advise_cb->cb = rwdts_member_data_advise_cb;

    rwdts_advise_query_proto_int(apih,
                                 merged_keyspec,
                                 NULL,
                                 msg,
                                 corrid,
                                 NULL,
                                 action,
                                 advise_cb,
                                 flags,
                                 reg,
                                 &xact);
  } else {
    rwdts_memer_data_adv_event_t* member_adv_event = rwdts_memer_data_adv_init(rcvd_advise_cb,
                                                                               reg,
                                                                               false,
                                                                               true);
    RW_ASSERT_TYPE(member_adv_event, rwdts_memer_data_adv_event_t);
    advise_cb->ud = member_adv_event;
    advise_cb->cb = rwdts_member_data_advise_cb;

    flags |= RWDTS_XACT_FLAG_TRANS_CRUD;

    rwdts_advise_append_query_proto(xact,
                                    merged_keyspec,
                                    NULL,
                                    msg,
                                    corrid,
                                    NULL,
                                    action,
                                    advise_cb,
                                    flags,
                                    reg);
  }

  // Free the keyspec
  RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL , NULL);
  return xact;
}

/*
 * Functions to handle member_data_record used to invoke
 * the async dispatch call for the various data member APIs.
 */

/* init a data member record */
static rwdts_member_data_record_t*
rwdts_member_data_record_init(rwdts_xact_t*                xact,
                              rwdts_member_registration_t* reg,
                              rw_keyspec_entry_t*          pe,
                              rw_schema_minikey_opaque_t*  mk,
                              const ProtobufCMessage*      msg,
                              uint32_t                     flags,
                              rw_keyspec_path_t*           keyspec,
                              rwdts_event_cb_t*            member_advise_cb)
{
  rwdts_member_data_record_t *mbr_data;

  mbr_data = RW_MALLOC0_TYPE(sizeof(rwdts_member_data_record_t), rwdts_member_data_record_t);

  mbr_data->xact = xact;
  mbr_data->reg  = reg;
  if (pe != NULL) {
    mbr_data->pe = (rw_keyspec_entry_t*)protobuf_c_message_duplicate(NULL, (ProtobufCMessage*)pe,
                     ((ProtobufCMessage*)pe)->descriptor);
  }
  if (mk != NULL) {
    mbr_data->mk = mk;
  }
  if (msg != NULL) {
    mbr_data->msg = protobuf_c_message_duplicate(NULL, msg, msg->descriptor);
  }
  mbr_data->flags = flags;
  mbr_data->keyspec = keyspec;
  if (member_advise_cb) {
   mbr_data->rcvd_member_advise_cb.cb = member_advise_cb->cb;
   mbr_data->rcvd_member_advise_cb.ud = member_advise_cb->ud;
  }

  RW_ASSERT(!msg || reg->desc == msg->descriptor);
  return mbr_data;
}

/* deinit a data member record */
static rw_status_t
rwdts_member_data_record_deinit(rwdts_member_data_record_t *mbr_data)
{
  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);


  mbr_data->xact  = NULL;
  mbr_data->reg   = NULL;
  if (mbr_data->pe) {
    protobuf_c_message_free_unpacked(NULL, (ProtobufCMessage*)mbr_data->pe);
    mbr_data->pe    = NULL;
  }
  /*
   * Deep free the message if present
   */
  if (mbr_data->msg) {
    protobuf_c_message_free_unpacked(NULL, mbr_data->msg);
    mbr_data->msg = NULL;
  }

  /*
   * free the keyspec
   */
  if (mbr_data->keyspec) {
    RW_KEYSPEC_PATH_FREE(mbr_data->keyspec, NULL , NULL);
    mbr_data->keyspec = NULL;
  }

  /* ATTN Free the mini key ???? */
  if (mbr_data->mk) {

  }

  RW_FREE_TYPE(mbr_data, rwdts_member_data_record_t);

  return RW_STATUS_SUCCESS;
}

/* expand a data member record */
static inline void
rwdts_expand_member_data_record(rwdts_member_data_record_t*   mbr_data,
                                rwdts_xact_t**                xact_p,
                                rwdts_member_registration_t** reg_p,
                                rw_keyspec_entry_t**          pe_p,
                                rw_schema_minikey_opaque_t**  mk,
                                ProtobufCMessage**            msg_p,
                                uint32_t*                     flags,
                                rw_keyspec_path_t**           keyspec)
{
  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);

  *xact_p  = mbr_data->xact;
  *reg_p   = mbr_data->reg;
  *pe_p    = mbr_data->pe;
  *msg_p   = mbr_data->msg;
  *flags   = mbr_data->flags;
  *mk      = mbr_data->mk;
  if (keyspec) {
    *keyspec = mbr_data->keyspec;
  }
  return;
}

typedef rw_status_t (*member_data_fn_t)(rwdts_member_data_record_t *mbr_data);

/*
 * define wrapper fn to invoke respective fn
 */
static rw_status_t
rwdts_member_data_invoke_w_cb(rwdts_member_reg_handle_t regh,
                              rw_keyspec_path_t*        keyspec,
                              const ProtobufCMessage*   msg,
                              rwdts_xact_t*             xact,
                              rwdts_event_cb_t*         member_advise_cb,
                              member_data_fn_t          member_data_fn)
{
  rwdts_member_data_record_t* mbr_data = NULL;
  rw_keyspec_path_t*          dup_keyspec =  NULL;
  rw_status_t rw_status;

  rw_status = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &dup_keyspec, NULL);
  RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
  RW_ASSERT(keyspec);

  mbr_data = rwdts_member_data_record_init(xact,
                                           (rwdts_member_registration_t*)regh,
                                           NULL, NULL,
                                           msg, 0,
                                           dup_keyspec,
                                           member_advise_cb);

  RW_ASSERT(mbr_data != NULL);

  return (member_data_fn(mbr_data));
}

rw_status_t
rwdts_member_data_block_advise_w_cb(rwdts_member_reg_handle_t      regh,
                                    RWDtsQueryAction               action,
                                    const rw_keyspec_path_t*       keyspec,
                                    const ProtobufCMessage*        msg,
                                    rwdts_xact_t*                  xact,
                                    rwdts_event_cb_t*              member_advise_cb)
{
  member_data_fn_t member_data_fn;

    switch (action) {
      case RWDTS_QUERY_CREATE:
        member_data_fn = rwdts_member_data_create_list_element_w_key_f;
        break;
      case RWDTS_QUERY_UPDATE:
        member_data_fn = rwdts_member_data_update_list_element_w_key_f;
        break;
      case RWDTS_QUERY_DELETE:
        member_data_fn = rwdts_member_data_delete_list_element_w_key_f;
        break;
      default:
        RW_CRASH();
    }
    return rwdts_member_data_invoke_w_cb(regh,
                                         (rw_keyspec_path_t*)keyspec,
                                         msg,
                                         xact,
                                         member_advise_cb,
                                         member_data_fn);

}

/*
 * Create a list element
 */
static rw_status_t
rwdts_member_data_create_list_element(rwdts_xact_t*             xact,
                                      rwdts_member_reg_handle_t regh,
                                      rw_keyspec_entry_t*       pe,
                                      const ProtobufCMessage*   msg)
{

  rwdts_member_data_record_t *mbr_data;

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, pe, NULL,
                                           msg, 0, NULL, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_create_list_element_f(mbr_data));
}

rw_status_t
rwdts_member_data_create_list_element_mk(rwdts_xact_t*                xact,
                                         rwdts_member_reg_handle_t    regh,
                                         rw_schema_minikey_opaque_t*  mk,
                                         const ProtobufCMessage*      msg)
{

  rwdts_member_data_record_t *mbr_data;

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, NULL, mk,
                                           msg, 0, NULL, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_create_list_element_f(mbr_data));
}

static rw_status_t
rwdts_member_data_create_list_element_f(rwdts_member_data_record_t *mbr_data)
{

  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);

  rwdts_xact_t* xact = NULL;
  rwdts_member_registration_t *reg = NULL;
  rw_keyspec_entry_t* pe = NULL;
  rw_schema_minikey_opaque_t* mk = NULL;
  ProtobufCMessage *msg = NULL;
  uint32_t flags = 0;
  rw_status_t rw_status = RW_STATUS_SUCCESS;
  rw_keyspec_path_t *merged_keyspec = NULL;

  rwdts_expand_member_data_record(mbr_data, &xact, &reg, &pe, &mk, &msg, &flags, &merged_keyspec);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  // Make sure this registration is  listy
  RW_ASSERT(rw_keyspec_path_is_listy(reg->keyspec) == true);
  
  RW_ASSERT(msg);

  if (rw_keyspec_path_is_listy(reg->keyspec) == false) {
    return RW_STATUS_FAILURE;
  }

  if (!msg) {
    return RW_STATUS_FAILURE;
  }

  if ((pe == NULL) && (mk == NULL)) {
    rw_status = rwdts_member_data_record_deinit(mbr_data);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
    return RW_STATUS_FAILURE;
  }

  // Make sure we have only one type key
  RW_ASSERT((pe == NULL) || (mk == NULL));

  if (merged_keyspec == NULL) {
    rw_status = rwdts_get_merged_keyspec(pe, mk, reg->keyspec, &merged_keyspec);
    if (rw_status == RW_STATUS_FAILURE) {
      RWDTS_MEMBER_SEND_ERROR(xact, reg->keyspec, NULL, NULL, NULL, rw_status,
                              RWDTS_ERRSTR_RSP_MERGE);
      RW_ASSERT(merged_keyspec == NULL);
      rw_status = rwdts_member_data_record_deinit(mbr_data);
      RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
      return RW_STATUS_SUCCESS;
    }
  }

  // mbr_data now owns the merged keyspec
  mbr_data->keyspec = merged_keyspec;

  return rwdts_member_data_create_list_element_w_key_f(mbr_data);
}

/*
 * Update a list element
 */
static rw_status_t
rwdts_member_data_update_list_element(rwdts_xact_t*             xact,
                                      rwdts_member_reg_handle_t regh,
                                      rw_keyspec_entry_t*       pe,
                                      const ProtobufCMessage*   msg,
                                      uint32_t                  flags)
{
  // Create a temp record to pass  using dispatch async to the DTS thread
  rwdts_member_data_record_t *mbr_data;

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, pe, NULL,
                                           msg, flags, NULL, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_update_list_element_f(mbr_data));
}

rw_status_t
rwdts_member_data_update_list_element_mk(rwdts_xact_t*                xact,
                                         rwdts_member_reg_handle_t    regh,
                                         rw_schema_minikey_opaque_t*  mk,
                                         const ProtobufCMessage*      msg,
                                         uint32_t                     flags)
{
  // Create a temp record to pass  using dispatch async to the DTS thread
  rwdts_member_data_record_t *mbr_data;

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, NULL, mk,
                                           msg, flags, NULL, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_update_list_element_f(mbr_data));
}

/*
 * Update a list element  - async flavour
 */
rw_status_t
rwdts_member_data_update_list_element_async(rwdts_xact_t*             xact,
                                            rwdts_member_reg_handle_t regh,
                                            rw_keyspec_entry_t*  pe,
                                            const ProtobufCMessage*   msg,
                                            uint32_t                  flags)
{
  // Create a temp record to pass  using dispatch async to the DTS thread
  rwdts_member_data_record_t *mbr_data;

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, pe, NULL,
                                           msg, flags, NULL, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT_TYPE(regh, rwdts_member_registration_t);
  rwdts_api_t* apih = ((rwdts_member_registration_t*)regh)->apih;

  rwsched_dispatch_async_f(apih->tasklet,
                           apih->client.rwq,
                           mbr_data,
                           (dispatch_function_t)rwdts_member_data_update_list_element_f);
  return RW_STATUS_SUCCESS;
}

/*
 * update a list element - internal
 */

rw_status_t
rwdts_member_data_update_list_element_f(rwdts_member_data_record_t *mbr_data)
{
  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);

  rwdts_xact_t* xact = NULL;
  rwdts_member_registration_t* reg = NULL;
  rw_keyspec_entry_t* pe = NULL;
  rw_schema_minikey_opaque_t* mk = NULL;
  ProtobufCMessage *msg = NULL;
  uint32_t flags = 0;
  rw_status_t rw_status = RW_STATUS_SUCCESS;
  rw_keyspec_path_t *merged_keyspec = NULL;
  rwdts_expand_member_data_record(mbr_data, &xact, &reg, &pe, &mk, &msg, &flags, &merged_keyspec);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  // Make sure this registration is  listy
  RW_ASSERT(rw_keyspec_path_is_listy(reg->keyspec) == true);
  RW_ASSERT(msg);

  if ((pe == NULL) && (mk == NULL)) {
    rw_status = rwdts_member_data_record_deinit(mbr_data);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT((pe == NULL) || (mk == NULL));

  if (merged_keyspec == NULL) {
    rw_status = rwdts_get_merged_keyspec(pe, mk, reg->keyspec, &merged_keyspec);
    if (rw_status == RW_STATUS_FAILURE) {
      RW_ASSERT(merged_keyspec == NULL);
      RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                              rw_status, RWDTS_ERRSTR_RSP_MERGE);
      rw_status = rwdts_member_data_record_deinit(mbr_data);
      RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
      return RW_STATUS_FAILURE;
    }
  }

  // mbr_data now consumes merged_keyspec
  RW_ASSERT(merged_keyspec);
  mbr_data->keyspec = merged_keyspec;

  return rwdts_member_data_update_list_element_w_key_f(mbr_data);

}

/*
 * Delete a list element
 */
rw_status_t
rwdts_member_data_delete_list_element(rwdts_xact_t*             xact,
                                      rwdts_member_reg_handle_t regh,
                                      rw_keyspec_entry_t*  pe,
                                      const ProtobufCMessage*   msg)
{
  // Create a temp record to pass  using dispatch async to the DTS thread
  rwdts_member_data_record_t *mbr_data;

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, pe, NULL,
                                           msg, 0, NULL, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_delete_list_element_f(mbr_data));
}

rw_status_t
rwdts_member_data_delete_list_element_mk(rwdts_xact_t*                xact,
                                         rwdts_member_reg_handle_t    regh,
                                         rw_schema_minikey_opaque_t*  mk,
                                         const ProtobufCMessage*      msg)
{
  // Create a temp record to pass  using dispatch async to the DTS thread
  rwdts_member_data_record_t *mbr_data;

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, NULL, mk,
                                           msg, 0, NULL, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_delete_list_element_f(mbr_data));
}


/*
 *  Delete a list element - internal
 */
static rw_status_t
rwdts_member_data_delete_list_element_f(rwdts_member_data_record_t *mbr_data)
{
  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);

  rwdts_xact_t* xact = NULL;
  rwdts_member_registration_t* reg = NULL;
  rw_keyspec_entry_t* pe = NULL;
  rw_schema_minikey_opaque_t* mk = NULL;
  ProtobufCMessage *msg = NULL;
  uint32_t flags = 0;
  rw_status_t rw_status = RW_STATUS_SUCCESS;
  rw_keyspec_path_t *merged_keyspec = NULL;

  rwdts_expand_member_data_record(mbr_data, &xact, &reg, &pe, &mk, &msg, &flags, &merged_keyspec);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  // Make sure this registration is  listy
  RW_ASSERT(rw_keyspec_path_is_listy(reg->keyspec) == true);

  // ATTN: per rajesh: invalid test: RW_ASSERT(msg);

  if ((pe == NULL) && (mk == NULL)) {
    rw_status = rwdts_member_data_record_deinit(mbr_data);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT((pe == NULL) || (mk == NULL));

  rw_status = rwdts_get_merged_keyspec(pe, mk, reg->keyspec, &merged_keyspec);
  if (rw_status == RW_STATUS_FAILURE) {
    if (merged_keyspec) {
      RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL , NULL);
    }
    rw_status = rwdts_member_data_record_deinit(mbr_data);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
    return RW_STATUS_FAILURE;
  }

  mbr_data->keyspec = merged_keyspec;
  return rwdts_member_data_delete_list_element_w_key_f(mbr_data);
}


rwdts_member_cursor_t*
rwdts_member_data_get_current_cursor(rwdts_xact_t* xact,
                                     rwdts_member_reg_handle_t regh)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  if (xact) {
    if (!xact->cursor) {
      xact->cursor = RW_MALLOC0_TYPE(sizeof(rwdts_member_cursor_impl_t), rwdts_member_cursor_impl_t);
      xact->cursor->xact = xact;
      RW_ASSERT(xact->cursor);
    }
    return ((rwdts_member_cursor_t *)xact->cursor);
  }
  if (!reg->cursor) {
    reg->cursor = RW_MALLOC0_TYPE(sizeof(rwdts_member_cursor_impl_t), rwdts_member_cursor_impl_t);
    RW_ASSERT(reg->cursor);
    reg->cursor->reg = reg;
  }
  return ((rwdts_member_cursor_t *)reg->cursor);
}
void
rwdts_member_data_reset_cursor(rwdts_xact_t* xact,
                               rwdts_member_reg_handle_t regh)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  if (xact) {
    if (xact->cursor) {
      xact->cursor->position = 0;
    }
  }
  if (reg->cursor) {
    reg->cursor->position = 0;
  }
}

/*
 *  get next list element
 */
const ProtobufCMessage*
rwdts_member_data_get_next_list_element(rwdts_xact_t*             xact,
                                        rwdts_member_reg_handle_t regh,
                                        rwdts_member_cursor_t*    cursorh,
                                        rw_keyspec_path_t**            keyspec)
{
  rwdts_member_registration_t* reg      = (rwdts_member_registration_t*)regh;
  rwdts_member_cursor_impl_t*  cursor   = (rwdts_member_cursor_impl_t*) cursorh;
  int                          pos      = 0;
  rwdts_member_data_object_t*  look_obj = NULL;
  rwdts_member_data_object_t*  data     = NULL;
  rwdts_member_data_object_t*  tmpdata  = NULL;
  rwdts_member_data_object_t*  obj      = NULL;

  RW_ASSERT_TYPE(cursor, rwdts_member_cursor_impl_t);
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (cursor->xact) {
    RW_ASSERT(xact == cursor->xact);
    RW_ASSERT_TYPE(xact, rwdts_xact_t);
  } else {
    RW_ASSERT(cursor->reg == reg);
  }

  // Start with the committed list


  // Find the next element based on the current cursor;
  // ATTN -- This need to be more efficient -- Save the pointer in cursor and manage deletes
  // ATTN -- How abt sorted by key -- this will now return in the order of insertion
  pos = 0;
  data = NULL, tmpdata = NULL;

  if (HASH_CNT(hh_data,reg->obj_list) > cursor->position) {
    HASH_ITER(hh_data, reg->obj_list, data, tmpdata) {
      if (pos++ == cursor->position) {
        cursor->position++;
        RW_ASSERT(data->keyspec);

        // If this cursor is xact based, then lookup to see if xact has modified this data
	look_obj = NULL;
        if (cursor->xact != NULL) {
          HASH_FIND(hh_data, xact->obj_list, data->key, data->key_len, look_obj);
        }
        if (look_obj) {
	  if (look_obj->delete_mark) {
	    /* Whoa, deleted in the transaction, skip to next one */
	    continue;
	  }
          obj = look_obj;
        } else {
          obj = data;
        }
        if (keyspec)
        {
          *keyspec = obj->keyspec;
        }
        return obj->msg;
      }
    }
  }

  pos = HASH_CNT(hh_data,reg->obj_list);
  data = NULL, tmpdata = NULL;

  // If this is xact based cursor - lookup in the xact list
  if (cursor->xact) {
    HASH_ITER(hh_data, xact->obj_list, data, tmpdata) {
      if (pos++ == cursor->position) {
        cursor->position++;
        RW_ASSERT(data->keyspec);

        /* Skip the entry marked for deletion */
        if (data->delete_mark) {
          continue;
        }

        //  Did we already give this object as part of the committed walk?
        HASH_FIND(hh_data, reg->obj_list, data->key, data->key_len, look_obj);

        if (look_obj) {
          /* Committed item, cannot be marked as a proposed deletion */
          RW_ASSERT(!look_obj->delete_mark);
          continue;
        }

        // Is this object really related to this registration?
        if (data && data->msg && data->msg->descriptor != reg->desc) {
          continue;
        }
        if (keyspec)
        {
          *keyspec = data->keyspec;
        }
        return  data->msg;
      }
    }
  }

  // Nothing found - reset the cursor and return NULL
  cursor->position = 0;
  return NULL;
}

rw_status_t
rwdts_member_data_get_list_element(rwdts_xact_t*             xact,
                                   rwdts_member_reg_handle_t regh,
                                   rw_keyspec_entry_t*       pe,
                                   rw_keyspec_path_t**       keyspec,
                                   const ProtobufCMessage**  out_msg)
{
  uint8_t*                     key = NULL;
  size_t                       key_len = 0;
  bool                         new_pe = TRUE;
  size_t                       entry_index;
  int                          key_index;
  rwdts_member_registration_t* reg = (rwdts_member_registration_t*)regh;
  rw_keyspec_path_t*           m_keyspec = NULL;
  rw_status_t                  rw_status;
  rwdts_api_t*                 apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT(pe);

  if (keyspec) *keyspec = NULL;
  if (out_msg) *out_msg = NULL;

  new_pe = RW_KEYSPEC_PATH_IS_ENTRY_AT_TIP(reg->keyspec, NULL, pe, &entry_index, &key_index, NULL);
  if (new_pe == FALSE) {
    m_keyspec = RW_KEYSPEC_PATH_CREATE_DUP_OF_TYPE(reg->keyspec, NULL,
                                                   &rw_schema_path_spec__descriptor, NULL);
    if (m_keyspec == NULL) {
      RWDTS_MEMBER_SEND_ERROR(xact, reg->keyspec, NULL, NULL, NULL,
                              RW_STATUS_FAILURE, RWDTS_ERRSTR_KEY_DUP);
      return RW_STATUS_FAILURE;
    }
    rw_status = RW_KEYSPEC_PATH_APPEND_ENTRY(m_keyspec, NULL , pe, NULL);
    if (rw_status != RW_STATUS_SUCCESS) {
      RWDTS_MEMBER_SEND_ERROR(xact, m_keyspec, NULL, NULL, NULL,
                              rw_status, RWDTS_ERRSTR_KEY_APPEND);
      RW_KEYSPEC_PATH_FREE(m_keyspec, NULL , NULL);
      return rw_status;
    }
  }
  else {
    rw_status = RW_KEYSPEC_PATH_CREATE_DUP(reg->keyspec, NULL , &m_keyspec, NULL);
    if (rw_status != RW_STATUS_SUCCESS) {
      RWDTS_MEMBER_SEND_ERROR(xact, reg->keyspec, NULL, NULL, NULL,
                              rw_status, RWDTS_ERRSTR_KEY_DUP);
      return rw_status;
    }
    if (entry_index != -1) {
      rw_status = RW_KEYSPEC_PATH_ADD_KEYS_TO_ENTRY(m_keyspec, NULL, pe, entry_index, NULL);
      if (rw_status == RW_STATUS_FAILURE) {
        RWDTS_MEMBER_SEND_ERROR(xact, m_keyspec, NULL, NULL, NULL,
                                rw_status, RWDTS_ERRSTR_KEY_APPEND);
        RW_KEYSPEC_PATH_FREE(m_keyspec, NULL , NULL);
        return rw_status;
      }
    }
  }

  if (rw_keyspec_path_has_wildcards(m_keyspec)) {
    RWDTS_MEMBER_SEND_ERROR(xact, m_keyspec, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_KEY_WILDCARDS);
    return RW_STATUS_FAILURE;
  }

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, m_keyspec, reg->keyspec);

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(m_keyspec, NULL,
                                          (const uint8_t **)&key, &key_len, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, m_keyspec, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_BINPATH);
    return rw_status;
  }
  RW_ASSERT(key);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_member_data_object_t **obj_list_p = NULL;

  if (xact != NULL) {
    // Create the object in the context of a transaction
    obj_list_p = &xact->obj_list;
  } else {
    // Create the object in the committed context of this member
    obj_list_p = &reg->obj_list;
  }

  RW_ASSERT(obj_list_p);

  // delete will fail, if there is no  existing object
  if (*obj_list_p == NULL) {
    RWDTS_API_LOG_EVENT(apih, DataMemberApiError, "Get list data element failed - obj list is empty");
    RW_KEYSPEC_PATH_FREE(m_keyspec, NULL , NULL);
    return RW_STATUS_FAILURE;
  }

  rwdts_member_data_object_t *mobj = NULL;

  HASH_FIND(hh_data, *obj_list_p, key, key_len, mobj);

  // Not found - lookup in the committed list
  if (mobj == NULL) {
    //ATTN please fix this
    HASH_FIND(hh_data, reg->obj_list, key, key_len, mobj);
  }
  if (mobj == NULL) {
    RW_KEYSPEC_PATH_FREE(m_keyspec, NULL , NULL);
    return RW_STATUS_FAILURE;
  }
  if (keyspec) {
    *keyspec = mobj->keyspec;
  } 

  RW_KEYSPEC_PATH_FREE(m_keyspec, NULL , NULL);

  if (out_msg) *out_msg = mobj->msg;
  return RW_STATUS_SUCCESS;
}

const ProtobufCMessage*
rwdts_member_data_get_list_element_mk(rwdts_xact_t*                xact,
                                      rwdts_member_reg_handle_t    regh,
                                      rw_schema_minikey_opaque_t*  mk)
{
  uint8_t *key = NULL;
  size_t key_len = 0;

  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  rw_keyspec_path_t *merged_keyspec = NULL;
  rw_status_t rw_status;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT(mk);

  if (!mk) {
    return NULL;
  }

  rw_status = RW_KEYSPEC_PATH_CREATE_DUP(reg->keyspec, NULL , &merged_keyspec, NULL);
  RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
  int depth = rw_keyspec_path_get_depth(merged_keyspec);
  rw_status = rw_schema_mk_copy_to_keyspec(merged_keyspec, mk, depth-1);
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_DUP);
    goto finish;
  }

  if (rw_keyspec_path_has_wildcards(merged_keyspec)) {
    rw_status = RW_STATUS_FAILURE;
    RWDTS_MEMBER_SEND_ERROR(xact, merged_keyspec, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_WILDCARDS);
    goto finish;
  }

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, merged_keyspec, reg->keyspec);

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(merged_keyspec, NULL ,
                                          (const uint8_t **)&key,
                                          &key_len, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, merged_keyspec, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_BINPATH);
    goto finish;
  }

  RW_ASSERT(key);

  rwdts_member_data_object_t **obj_list_p = NULL;

  // ATTN - Check with tom to see how yo find of the registration
  // at a non list boundary

  if (xact != NULL) {
    // Create the object in the context of a transaction
    obj_list_p = &xact->obj_list;
  } else {
    // Create the object in the committed context of this member
    obj_list_p = &reg->obj_list;
  }

  RW_ASSERT(obj_list_p);

  // delete will fail, if there is no  existing object
  if (*obj_list_p == NULL) {
    RWDTS_API_LOG_EVENT(apih, DataMemberApiError, "Get list data element failed - obj list is empty");
    goto finish;
  }
  rwdts_member_data_object_t *mobj = NULL;

  HASH_FIND(hh_data, (*obj_list_p), key, key_len, mobj);

  if (mobj == NULL) {
    HASH_FIND(hh_data, reg->obj_list, key, key_len, mobj);
    goto finish;
  }

  RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL , NULL);
  return mobj->msg;

finish:
  if (merged_keyspec) {
    RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL , NULL);
  }
  return NULL;
}

/*
 * Get the data count in this registration
 */
uint32_t
rwdts_member_data_get_count(rwdts_xact_t*             xact,
                            rwdts_member_reg_handle_t  regh)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  RW_ASSERT(reg);
  if (!reg) {
    return 0;
  }

  if (xact != NULL) {
    return (HASH_CNT(hh_data, xact->obj_list));
  }
  return (HASH_CNT(hh_data, reg->obj_list));
}

/*
 * Create a list element with keyspec
 */
static rw_status_t
rwdts_member_data_create_list_element_w_key(rwdts_xact_t*             xact,
                                            rwdts_member_reg_handle_t regh,
                                            rw_keyspec_path_t*        keyspec,
                                            const ProtobufCMessage*   msg)
{

  rwdts_member_data_record_t *mbr_data;
  rw_keyspec_path_t *merged_keyspec = NULL;
  rw_status_t rw_status;

  rw_status = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &merged_keyspec, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_DUP);
    return rw_status;
  }

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, NULL, NULL,
                                           msg, 0, merged_keyspec, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, merged_keyspec, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL , NULL);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_create_list_element_w_key_f(mbr_data));
}

static rw_status_t
rwdts_member_data_create_list_element_w_key_f(rwdts_member_data_record_t *mbr_data)
{
  rwdts_member_data_object_t*  mobj       = NULL;
  rwdts_member_data_object_t*  new_mobj   = NULL;
  rwdts_member_data_object_t** obj_list_p = NULL;
  rwdts_api_t*                 apih       = NULL;
  const uint8_t*               key        = NULL;
  size_t                       key_len    = 0;
  rw_status_t                  rw_status;
  uint8_t                      free_obj   = 0;

  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);
  RW_ASSERT(mbr_data->reg     != NULL);
  RW_ASSERT(mbr_data->msg     != NULL);
  RW_ASSERT(mbr_data->keyspec != NULL);

  if (mbr_data->reg == NULL) {
    return RW_STATUS_FAILURE;
  }
  if (mbr_data->msg == NULL) {
    return RW_STATUS_FAILURE;
  }
  if (mbr_data->keyspec == NULL) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT_TYPE(mbr_data->reg, rwdts_member_registration_t);
  if (rw_keyspec_path_has_wildcards(mbr_data->keyspec)) {
    if (mbr_data->xact != NULL) {
      RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                              NULL, rw_status, RWDTS_ERRSTR_KEY_WILDCARDS);
      return RW_STATUS_FAILURE;
    }
    else {
      RW_ASSERT_MESSAGE(0, RWDTS_ERRSTR_KEY_WILDCARDS);
    }
  }

  apih = mbr_data->reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (mbr_data->xact != NULL) {
    // Create the object in the context of a transaction
    obj_list_p = &mbr_data->xact->obj_list;
  } else {
    // Create the object in the committed context of this member
    obj_list_p = &mbr_data->reg->obj_list;
  }
  RW_ASSERT(obj_list_p);

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, mbr_data->keyspec, mbr_data->reg->keyspec);

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(mbr_data->keyspec, NULL ,
                                          (const uint8_t **)&key,
                                          &key_len, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    if (mbr_data->xact) {
      RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                              NULL, rw_status, RWDTS_ERRSTR_KEY_BINPATH);
    }
    else {
      RW_ASSERT_MESSAGE(0, RWDTS_ERRSTR_KEY_BINPATH);
    }
    goto finish;
  }

  HASH_FIND(hh_data, (*obj_list_p), key, key_len, mobj);

  if (mobj == NULL && mbr_data->xact != NULL) {
   // Check committed data
    HASH_FIND(hh_data, mbr_data->reg->obj_list, key, key_len, mobj);
  }

  if (mobj != NULL) {
    /* May include a deleted item, in which case our create is also an update */
    return (rwdts_member_data_update_list_element_w_key_f(mbr_data));
  }

  // Strip unknown in inbound Pb
  if (mbr_data->msg) {
    if (protobuf_c_message_unknown_get_count(mbr_data->msg)) {
      rwdts_report_unknown_fields(mbr_data->keyspec, apih, mbr_data->msg);
      protobuf_c_message_delete_unknown_all(NULL, mbr_data->msg);
      RW_ASSERT(!protobuf_c_message_unknown_get_count(mbr_data->msg))
    }
  }

  new_mobj = rwdts_member_data_store(obj_list_p, mbr_data, &free_obj);
  if (!new_mobj) {
    rw_status = RW_STATUS_FAILURE;
    if (mbr_data->xact) {
      RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec,
                              NULL, NULL, NULL,
                              rw_status, "Did not get the new object in list");
    }
    goto finish;
  }

  /* Add audit trail, update data store, increment serial and send advise as necessary */
  rwdts_member_data_finalize(mbr_data,
                             new_mobj,
                             RWDTS_QUERY_CREATE,
                             RW_DTS_AUDIT_TRAIL_ACTION_OBJ_CREATE);

  if (free_obj) {
    rw_status = rwdts_member_data_deinit(new_mobj);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
  }

finish:
  rwdts_member_data_record_deinit(mbr_data);
  return rw_status;
}


/*
 * Update a list element with key
 */
rw_status_t
rwdts_member_data_update_list_element_w_key(rwdts_xact_t*             xact,
                                            rwdts_member_reg_handle_t regh,
                                            rw_keyspec_path_t*        keyspec,
                                            const ProtobufCMessage*   msg,
                                            uint32_t                  flags)
{
  rwdts_member_data_record_t* mbr_data = NULL;
  rw_keyspec_path_t*               dup_keyspec =  NULL;
  rw_status_t rw_status;

  if (!keyspec) {
    return RW_STATUS_FAILURE;
  }
  rw_status = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &dup_keyspec, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_DUP);
    return rw_status;
  }

  mbr_data = rwdts_member_data_record_init(xact,
                                           (rwdts_member_registration_t*)regh,
                                           NULL,
                                           NULL,
                                           msg,
                                           flags,
                                           dup_keyspec, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, dup_keyspec, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    RW_KEYSPEC_PATH_FREE(dup_keyspec, NULL , NULL);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_update_list_element_w_key_f(mbr_data));
}

/*
 * Update a list element with key - async flavour
 */
rw_status_t
rwdts_member_data_update_list_element_w_key_async(rwdts_xact_t*             xact,
                                                  rwdts_member_reg_handle_t regh,
                                                  rw_keyspec_path_t*        keyspec,
                                                  const ProtobufCMessage*   msg,
                                                  uint32_t                  flags)
{
  // Create a temp record to pass  using dispatch async to the DTS thread
  rwdts_member_data_record_t* mbr_data       = NULL;
  rw_keyspec_path_t*               dup_keyspec    = NULL;
  rwdts_api_t*                apih           = NULL;
  rw_status_t                 rw_status;

  RW_ASSERT_TYPE(regh, rwdts_member_registration_t);
  apih = ((rwdts_member_registration_t*)regh)->apih;

  rw_status = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &dup_keyspec, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_DUP);
    return rw_status;
  }

  mbr_data = rwdts_member_data_record_init(xact,
                                           (rwdts_member_registration_t*)regh,
                                           NULL,
                                           NULL,
                                           msg,
                                           flags,
                                           dup_keyspec, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, dup_keyspec, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    RW_KEYSPEC_PATH_FREE(dup_keyspec, NULL , NULL);
    return RW_STATUS_FAILURE;
  }

  rwsched_dispatch_async_f(apih->tasklet,
                           apih->client.rwq,
                           mbr_data,
                           (dispatch_function_t)rwdts_member_data_update_list_element_w_key_f);

  return RW_STATUS_SUCCESS;
}

/*
 * update a list element with key - internal
 */

rw_status_t
rwdts_member_data_update_list_element_w_key_f(rwdts_member_data_record_t *mbr_data)
{
  rwdts_member_data_object_t** obj_list_p = NULL;
  rwdts_member_data_object_t*  mobj       = NULL;
  rwdts_member_data_object_t*  c_mobj     = NULL;
  rwdts_api_t*  apih                      = NULL;
  rw_status_t   rw_status                 = RW_STATUS_SUCCESS;
  const uint8_t* key                      = NULL;
  size_t key_len                          = 0;

  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);
  RW_ASSERT_TYPE(mbr_data->reg, rwdts_member_registration_t);
  RW_ASSERT(mbr_data->keyspec);
  RW_ASSERT(mbr_data->msg);

  if (!mbr_data->keyspec) {
    return RW_STATUS_FAILURE;
  }
  if (!mbr_data->msg) {
    return RW_STATUS_FAILURE;
  }


  if (rw_keyspec_path_has_wildcards(mbr_data->keyspec)) {
    if (mbr_data->xact) {
      rw_status = RW_STATUS_FAILURE;
      RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                              NULL, rw_status, RWDTS_ERRSTR_KEY_BINPATH);
      return rw_status;
    }
    else {
      RW_ASSERT_MESSAGE(0, RWDTS_ERRSTR_KEY_WILDCARDS);
    }
  }

  apih = mbr_data->reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (mbr_data->xact != NULL) {
    // Create the object in the context of a transaction
    obj_list_p = &mbr_data->xact->obj_list;
  } else {
    // Create the object in the committed context of this member
    obj_list_p = &mbr_data->reg->obj_list;
  }
  RW_ASSERT(obj_list_p);

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, mbr_data->keyspec,
                                         mbr_data->reg->keyspec);

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(mbr_data->keyspec, NULL,
                                          (const uint8_t **)&key, &key_len, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    if (mbr_data->xact) {
      RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                              NULL, rw_status, RWDTS_ERRSTR_KEY_BINPATH);
      return rw_status;
    }
    else {
      RW_ASSERT_MESSAGE(0, RWDTS_ERRSTR_KEY_BINPATH);
    }
  }

  rwdts_shard_handle_t* shard = mbr_data->reg->shard;

  if (shard && shard->appdata) {
    rw_status = rwdts_shard_handle_appdata_update_element(shard, mbr_data->keyspec, mbr_data->msg);
    if (rw_status == RW_STATUS_SUCCESS) {
      mobj = rwdts_member_data_init_frm_mbr_data(mbr_data, false, false, RWDTS_UPDATE);

      /* Add audit trail, update data store, increment serial and send advise as necessary */
      rwdts_member_data_finalize(mbr_data,
                                 mobj,
                                 RWDTS_QUERY_UPDATE,
                                 RW_DTS_AUDIT_TRAIL_ACTION_OBJ_UPDATE);
      rw_status = rwdts_member_data_deinit(mobj);
      RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
    }

    /* The following function consumes ebverything under mbr_data including  merged_keyspec */
    rw_status_t rs  = rwdts_member_data_record_deinit(mbr_data);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    return RW_STATUS_SUCCESS;
  }

  HASH_FIND(hh_data, (*obj_list_p), key, key_len, mobj);

  if (mobj == NULL) {
    if (mbr_data->xact != NULL) {
      // Check committed data

      HASH_FIND(hh_data, mbr_data->reg->obj_list, key, key_len, c_mobj);
      if (c_mobj != NULL) {
        /* Add an entry to xact data; it will be updated below */
        mobj = rwdts_member_dup_mobj(c_mobj);
        RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);
        HASH_ADD_KEYPTR(hh_data, (*obj_list_p), mobj->key, mobj->key_len, mobj);
      } else {
        /* Call create with the incoming msg */
        return (rwdts_member_data_create_list_element_w_key_f(mbr_data));
      }
    } else { // xact == NULL
      /* There is no existing mobj in commit data, and no xact.
         Adjust commit data to taste. */
      RW_ASSERT(obj_list_p == &mbr_data->reg->obj_list);

      /* Ignore REPLACE flag, just create if there wasn't one, that's
         what the xact flavor does.  Unclear if this is kosher
         inasmuch as we had one path doing one thing and the other the
         opposite! */
      return (rwdts_member_data_create_list_element_w_key_f(mbr_data));

      /* This former REPLACE-specific path appears to create an entry
         that goes on below to get merged with itself.  Unclear if
         that's advisable. */
      if (0) {
        mobj = rwdts_member_data_init_frm_mbr_data(mbr_data, false, false, RWDTS_UPDATE);
        RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);
        HASH_ADD_KEYPTR(hh_data, (*obj_list_p), mobj->key, mobj->key_len, mobj);
      }
    }
  }

  // Strip unknown in inbound Pb
  if (mbr_data->msg) {
    if (protobuf_c_message_unknown_get_count(mbr_data->msg)) {
      rwdts_report_unknown_fields(mbr_data->keyspec, apih, mbr_data->msg);
      protobuf_c_message_delete_unknown_all(NULL, mbr_data->msg);
      RW_ASSERT(!protobuf_c_message_unknown_get_count(mbr_data->msg))
    }
  }

  // Lookup the data
  HASH_FIND(hh_data, (*obj_list_p), key, key_len, mobj);

  RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);

  rwdts_member_data_merge_store(mbr_data, mobj);

  /* Add audit trail, update data store, increment serial and send advise as necessary */
  rwdts_member_data_finalize(mbr_data,
                             mobj,
                             RWDTS_QUERY_UPDATE,
                             RW_DTS_AUDIT_TRAIL_ACTION_OBJ_UPDATE);

  /* The following function consumes ebverything under mbr_data including  merged_keyspec */
  rw_status_t rs  = rwdts_member_data_record_deinit(mbr_data);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  return RW_STATUS_SUCCESS;
}

/*
 * Delete a list element with key
 */
static rw_status_t
rwdts_member_data_delete_list_element_w_key(rwdts_xact_t*             xact,
                                            rwdts_member_reg_handle_t regh,
                                            rw_keyspec_path_t*             keyspec,
                                            const ProtobufCMessage*   msg)
{
  // Create a temp record to pass  using dispatch async to the DTS thread
  rwdts_member_data_record_t *mbr_data;
  rw_keyspec_path_t *merged_keyspec = NULL;
  rw_status_t rw_status;

  rw_status = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &merged_keyspec, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_DUP);
    return rw_status;
  }

  mbr_data = rwdts_member_data_record_init(xact, (rwdts_member_registration_t*)regh, NULL, NULL,
                                           msg, 0, merged_keyspec, NULL);

  if (mbr_data == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, merged_keyspec, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, RWDTS_ERRSTR_MEMB_CREATE);
    RW_KEYSPEC_PATH_FREE(merged_keyspec, NULL , NULL);
    return RW_STATUS_FAILURE;
  }

  return (rwdts_member_data_delete_list_element_w_key_f(mbr_data));
}

/*
 *  Delete a list element with key - internal
 */
static rw_status_t
rwdts_member_data_delete_list_element_w_key_f(rwdts_member_data_record_t *mbr_data)
{

  rwdts_member_data_object_t** obj_list_p = NULL;
  rwdts_member_data_object_t*  mobj       = NULL;
  rwdts_api_t*  apih                      = NULL;
  rw_status_t   rw_status                 = RW_STATUS_SUCCESS;
  const uint8_t* key                      = NULL;
  size_t key_len                          = 0;
  bool update_advice                      = false;
  rw_keyspec_path_t *derived_ks = NULL;

  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);
  RW_ASSERT_TYPE(mbr_data->reg, rwdts_member_registration_t);
  RW_ASSERT(mbr_data->keyspec);
  if (!mbr_data->keyspec) {
    return RW_STATUS_FAILURE;
  }

  apih = mbr_data->reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, mbr_data->keyspec, mbr_data->reg->keyspec);

  if (mbr_data->xact != NULL) {
    // Delete the object in the context of a transaction
    obj_list_p = &mbr_data->xact->obj_list;
  } else {
    // Create the object in the committed context of this member
    obj_list_p = &mbr_data->reg->obj_list;
  }

  RW_ASSERT(obj_list_p);
  if (obj_list_p == NULL) {
    goto finish;
  }

  size_t depth_i = rw_keyspec_path_get_depth(mbr_data->keyspec);
  size_t depth_o = rw_keyspec_path_get_depth(mbr_data->reg->keyspec);

  rw_status= rw_keyspec_path_create_dup(mbr_data->keyspec, NULL, &derived_ks);
  RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
  if (rw_status != RW_STATUS_SUCCESS) {
    goto finish;
  }
  RW_ASSERT(derived_ks);

  /* The basic assumption accepted here is that there will not be wildcard in the
   * middle of the query-keyspec */

  if (depth_i > depth_o) {
    /* The duplicate of received keyspec is truncated at the registration point.
     * This is done to fetch the stored-object based on the received key. The
     * received query keyspec is intact in mbr_data->keyspec. This is later used
     * to delete the photo field from the stored-object data.
     */
    rw_status = rw_keyspec_path_trunc_suffix_n(derived_ks, NULL, depth_o);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
    if (rw_status != RW_STATUS_SUCCESS) {
      goto finish;
    }

    if (mbr_data->xact == NULL) {
      rw_status = RW_KEYSPEC_PATH_GET_BINPATH(derived_ks, NULL , (const uint8_t **)&key, &key_len, NULL);
      RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
      RW_ASSERT(key);
      mobj = NULL;
      /* Fetch the stored-object based on truncated query-keyspec */
      HASH_FIND(hh_data, (*obj_list_p), key, key_len, mobj);
      if (mobj) {
        RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);
        /* Delete the proto field from the stored-object based on received
         * (non-truncated) query keyspec */
        rw_status = RW_KEYSPEC_PATH_DELETE_PROTO_FIELD(NULL,
                                                       mobj->keyspec,
                                                       mbr_data->keyspec,
                                                       mobj->msg,
                                                       NULL);
        if (rw_status != RW_STATUS_SUCCESS) {
          char  *tmp_str = NULL;
          rw_keyspec_path_get_new_print_buffer(mbr_data->keyspec, NULL ,
                                               rwdts_api_get_ypbc_schema(apih),
                                               &tmp_str);
          RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, DeleteNonExistent, "Attempting to delete non existent data",
                                         apih->client_path, mbr_data->reg->keystr, tmp_str);
          if (tmp_str) {
            free(tmp_str);
          }
          goto finish;
        }
        update_advice = true;
      } else {
        RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                                NULL, rw_status,
                                "Delete list element failed; no such object");
        goto finish;
      }
    } else {
      rwdts_member_data_object_t *newobj = NULL;
      /* If the query keyspec is deeper than the registered keyspec, it needs to
         be truncated to find the object. Most of this code is temporary
         and will be modified when protobuf delta is available */
      if (rw_keyspec_path_has_wildcards(derived_ks)) {
        RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                                NULL, RW_STATUS_FAILURE,
                                RWDTS_ERRSTR_KEY_WILDCARDS);
        goto finish;
      }
      rw_status = RW_KEYSPEC_PATH_GET_BINPATH(derived_ks, NULL , (const uint8_t **)&key, &key_len, NULL);
      RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
      RW_ASSERT(key);
      mobj = NULL;

      /* Find the stored data-object from the committed list */
      HASH_FIND(hh_data, mbr_data->reg->obj_list, key, key_len, mobj);
      if (mobj) {
        RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);
        newobj = rwdts_member_data_duplicate_obj(mobj);
        /* Delete the proto field from the stored-object based on received
         * (non-truncated) query keyspec */
        rw_status = RW_KEYSPEC_PATH_DELETE_PROTO_FIELD(NULL,
                                                       mobj->keyspec,
                                                       mbr_data->keyspec,
                                                       newobj->msg,
                                                       NULL);
        if (rw_status != RW_STATUS_SUCCESS) {
          char  *tmp_str = NULL;
          rw_keyspec_path_get_new_print_buffer(mbr_data->keyspec, NULL ,
                                               rwdts_api_get_ypbc_schema(apih),
                                               &tmp_str);
          RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, DeleteNonExistent, "Attempting to delete non existent data",
                                         apih->client_path, mbr_data->reg->keystr, tmp_str);
          if (tmp_str) {
            free(tmp_str);
          }
          goto finish;
        }
        newobj->replace_flag = 1;
        newobj->update_flag = 1;
        HASH_ADD_KEYPTR(hh_data, (*obj_list_p), newobj->key, newobj->key_len, newobj);
        apih->stats.num_xact_update_objects++;
        mbr_data->reg->stats.num_xact_update_objects++;
      }

      mobj = NULL;
      /* Find the stored data-object from the non-committed list */
      HASH_FIND(hh_data, mbr_data->xact->obj_list, key, key_len, mobj);
      if (mobj) {
        RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);
        /* Delete the proto field from the temporarily stored data based on the
         * received (non-truncated) query keyspec */
        if (mobj->create_flag || mobj->update_flag) {
          rw_status = RW_KEYSPEC_PATH_DELETE_PROTO_FIELD(NULL,
                                                         mobj->keyspec,
                                                         mbr_data->keyspec,
                                                         mobj->msg,
                                                         NULL);
          if (rw_status != RW_STATUS_SUCCESS) {
            char  *tmp_str = NULL;
            rw_keyspec_path_get_new_print_buffer(mbr_data->keyspec, NULL ,
                                                 rwdts_api_get_ypbc_schema(apih),
                                                 &tmp_str);
            RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, DeleteNonExistent, "Attempting to delete non existent data",
                                           apih->client_path, mbr_data->reg->keystr, tmp_str);
            if (tmp_str) {
              free(tmp_str);
            }
            goto finish;
          }
          update_advice = true;
        }
      } 

      if (update_advice != true) { 
        RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                                NULL, rw_status,
                                "Delete list element failed; no such object");
        goto finish;
      }
    }
    goto send_update;
  }

  if (((depth_i == depth_o) && rw_keyspec_path_has_wildcards(mbr_data->keyspec)) ||
      (depth_i < depth_o)) {
      /* Remove the stored-data objects from commited and non-commited list in
       * this scenario when the query keyspec depth matches the reg keyspec
       * depth and the query keyspec is wildcarded. Also the same functionality
       * is needed when the query keyspec depth is less than the reg keyspec
       * depth */
      if (mbr_data->xact == NULL) {
        rwdts_member_data_object_t *mobj, *tmp;
        HASH_ITER(hh_data, mbr_data->reg->obj_list, mobj, tmp) {
          if (rw_keyspec_path_is_a_sub_keyspec(NULL, mbr_data->keyspec, mobj->keyspec)) {
            HASH_DELETE(hh_data, mbr_data->reg->obj_list, mobj);
            rwdts_member_pub_del(mbr_data, mobj);
            rw_status = rwdts_member_data_deinit(mobj);
            RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
            if (rw_status != RW_STATUS_SUCCESS) {
              goto finish;
            }
          }
        }
      } else {
        rwdts_member_data_object_t *mobj, *tmp;
        HASH_ITER(hh_data, mbr_data->reg->obj_list, mobj, tmp) {
          if (rw_keyspec_path_is_a_sub_keyspec(NULL, mbr_data->keyspec, mobj->keyspec)) {
            rwdts_member_data_object_t *newobj = NULL;
            newobj = rwdts_member_data_duplicate_obj(mobj);
            newobj->delete_mark = 1;
            HASH_ADD_KEYPTR(hh_data, (*obj_list_p), newobj->key, newobj->key_len, newobj);
            apih->stats.num_xact_delete_objects++;
            mbr_data->reg->stats.num_xact_delete_objects++;
          }
        }
        /* Remove any uncommited data matching the same key */
        HASH_ITER(hh_data, mbr_data->xact->obj_list, mobj, tmp) {
          if ((mobj->create_flag || mobj->update_flag) &&
              rw_keyspec_path_is_a_sub_keyspec(NULL, mbr_data->keyspec, mobj->keyspec)) {
            HASH_DELETE(hh_data, mbr_data->xact->obj_list, mobj);
            rw_status = rwdts_member_data_deinit(mobj);
            RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
            if (rw_status != RW_STATUS_SUCCESS) {
              goto finish;
            }
          }
        }
      }
    } else {
      /* This is a case where the depth_i and depth_o are same and the query
       * keyspec is not wildcarded. In this case, the data is fred when there is
       * an absolute key matching */
      rw_status = RW_KEYSPEC_PATH_GET_BINPATH(mbr_data->keyspec, NULL,
                                            (const uint8_t **)&key, &key_len, NULL);
      if (rw_status != RW_STATUS_SUCCESS) {
        if (mbr_data->xact) {
          RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                                  NULL, RW_STATUS_FAILURE, RWDTS_ERRSTR_KEY_BINPATH);
          goto finish;
        }
        else {
          RW_ASSERT_MESSAGE(0, RWDTS_ERRSTR_KEY_WILDCARDS);
        }
      }
      RW_ASSERT(key);
      if (key == NULL) {
        goto finish;
      }

      if (!mbr_data->xact && mbr_data->reg->shard && mbr_data->reg->shard->appdata) {
        rw_status = rwdts_shard_handle_appdata_delete_element(mbr_data->reg->shard, mbr_data->keyspec, mbr_data->msg);
        if (rw_status == RW_STATUS_SUCCESS) {
          mobj = rwdts_member_data_init_frm_mbr_data(mbr_data, false, false, RWDTS_DELETE);
          rwdts_member_pub_del(mbr_data, mobj);
          rw_status = rwdts_member_data_deinit(mobj);
          RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
          goto finish;
        }
      }
      HASH_FIND(hh_data, (*obj_list_p), key, key_len, mobj);

      if (mobj == NULL) {
        if (mbr_data->xact != NULL) {
          // Lookup in the committed list and create if it is not already there
          HASH_FIND(hh_data, mbr_data->reg->obj_list, key, key_len, mobj);
        }
        if (mobj) {
          // create a new mobj object in xact
          mobj = rwdts_member_data_init_frm_mbr_data(mbr_data, false, false, RWDTS_DELETE);
          RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);
          RW_ASSERT(mobj->keyspec);
          HASH_ADD_KEYPTR(hh_data, (*obj_list_p), mobj->key, mobj->key_len, mobj);
          HASH_FIND(hh_data, (*obj_list_p), key, key_len, mobj);
        } else {
          RWDTS_API_LOG_EVENT(apih, DataMemberApiError, "Delete list element failed - cannot find  object");
          rw_status =  RW_STATUS_FAILURE;
          if (mbr_data->xact != NULL) {
            RWDTS_MEMBER_SEND_ERROR(mbr_data->xact, mbr_data->keyspec, NULL, NULL,
                                    NULL, rw_status,
                                    "Delete list element failed");
          }
          goto finish;
        }
      }
      RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);
      if (mbr_data->xact == NULL) {
        // Delete this from the committed list
        HASH_DELETE(hh_data, (*obj_list_p), mobj);
        rwdts_member_pub_del(mbr_data, mobj);
        rw_status = rwdts_member_data_deinit(mobj);
        RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
      } else {
        mobj->delete_mark = 1;
        apih->stats.num_xact_delete_objects++;
        mbr_data->reg->stats.num_xact_delete_objects++;
        rwdts_member_pub_del(mbr_data, mobj);
      }
    }

send_update:
  if (update_advice) {
    if ((mbr_data->reg->flags & RWDTS_FLAG_PUBLISHER) != 0) {
      uint32_t flags = RWDTS_XACT_FLAG_REPLACE;
      if (mbr_data->member_advise_cb.cb)  {
        flags |= RWDTS_XACT_FLAG_EXECNOW;
      }
      rwdts_member_data_advice_query_action(mbr_data->xact,
                                            (rwdts_member_reg_handle_t)mobj->reg,
                                            mobj->keyspec,
                                            mobj->msg,
                                            RWDTS_QUERY_UPDATE,
                                            flags,
                                            &mbr_data->member_advise_cb,
                                            &mbr_data->rcvd_member_advise_cb,
                                            true);
    if (!mbr_data->xact && (mbr_data->reg->apih->rwtasklet_info &&
        ((mbr_data->reg->apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_BDB) ||
        (mbr_data->reg->apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_REDIS)))) {
        if (mbr_data->reg->kv_handle) {
          uint8_t *payload;
          size_t  payload_len;
          payload = protobuf_c_message_serialize(NULL, mobj->msg, &payload_len);
          rw_status_t rs = rwdts_kv_handle_add_keyval(mbr_data->reg->kv_handle, 0, (char *)mobj->key, mobj->key_len,
                                                     (char *)payload, (int)payload_len, rwdts_member_data_cache_set_callback, 
                                                     (void *)mbr_data->reg->kv_handle);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
          if (rs != RW_STATUS_SUCCESS) {
            /* Fail the VM and switchover to STANDBY */
          }
        }
      }
    }
    if ((mbr_data->reg->flags & RWDTS_FLAG_DATASTORE) != 0) {
      if (mbr_data->xact != NULL) {
        rwdts_kv_update_db_xact_precommit(mobj, RWDTS_QUERY_UPDATE);
      } else {
        rwdts_kv_update_db_update(mobj, RWDTS_QUERY_UPDATE);
      }
    }
  }

// Do not return without coming to finish -- there is cleanup here
//
finish:
  {
    rw_status = rwdts_member_data_record_deinit(mbr_data);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
  }

  if (derived_ks) {
    rw_keyspec_path_free(derived_ks, NULL);

  }

  return rw_status;
}

rw_status_t
rwdts_member_data_get_list_element_w_key(rwdts_xact_t*             xact,
                                         rwdts_member_reg_handle_t regh,
                                         rw_keyspec_path_t*        obj_keyspec,
                                         rw_keyspec_path_t**       out_keyspec,
                                         const ProtobufCMessage**  out_msg)
{
  uint8_t *key = NULL;
  size_t key_len = 0;

  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  rw_status_t rw_status;

  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT(obj_keyspec);
  if (!obj_keyspec) {
    return RW_STATUS_FAILURE;
  }

  if (out_keyspec) *out_keyspec = NULL;
  if (out_msg)     *out_msg     = NULL;

  /* Force the category to match registration ks category */
  RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, obj_keyspec, reg->keyspec);

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(obj_keyspec, NULL,
                                          (const uint8_t **)&key, &key_len,
                                          NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, obj_keyspec, NULL, NULL, NULL,
                            rw_status, RWDTS_ERRSTR_KEY_BINPATH);
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(key);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  if (reg->shard && reg->shard->appdata) {
    ProtobufCMessage *msg;
    if (out_keyspec) {
      rw_keyspec_path_t *merged_keyspec = NULL;
      rw_status_t rw_status = RW_KEYSPEC_PATH_CREATE_DUP(obj_keyspec, NULL,
                                                         &merged_keyspec, NULL);
      if (rw_status != RW_STATUS_SUCCESS) {
        RWDTS_MEMBER_SEND_ERROR(xact, obj_keyspec, NULL, NULL, NULL,
                                rw_status, RWDTS_ERRSTR_KEY_DUP);
        return rw_status;
      }
      *out_keyspec = merged_keyspec;
    }
    rw_status = rwdts_shard_handle_appdata_get_element(reg->shard, obj_keyspec, &msg);
    if ((rw_status == RW_STATUS_SUCCESS) && msg) {
      *out_msg = protobuf_c_message_duplicate(NULL, msg, msg->descriptor); 
    }
    return rw_status;
  }

  rwdts_member_data_object_t **obj_list_p = NULL;

  // ATTN - Check with tom to see how yo find of the registration
  // at a non list boundary

  if (xact != NULL) {
    // Create the object in the context of a transaction
    obj_list_p = &xact->obj_list;
  } else {
    // Create the object in the committed context of this member
    obj_list_p = &reg->obj_list;
  }

  RW_ASSERT(obj_list_p);

  // delete will fail, if there is no  existing object
  if (*obj_list_p == NULL) {
    RWDTS_API_LOG_EVENT(apih, DataMemberApiError,
                        "Get list data element failed - obj list is empty");
    if (xact) {
      RWDTS_MEMBER_SEND_ERROR(xact, obj_keyspec, NULL, NULL, NULL,
                              RW_STATUS_NOTFOUND,
                              "Get list data element failed - obj list is empty");
    }
    return RW_STATUS_NOTFOUND;
  }
  rwdts_member_data_object_t *mobj = NULL;

  HASH_FIND(hh_data, (*obj_list_p), key, key_len, mobj);

  // Not found - lookup in the committed list
  if (mobj == NULL) {
    HASH_FIND(hh_data, reg->obj_list, key, key_len, mobj);
  }


  if (mobj == NULL || mobj->delete_mark) {
    if (xact) {
      RWDTS_MEMBER_SEND_ERROR(xact, obj_keyspec, NULL, NULL, NULL,
                              RW_STATUS_NOTFOUND,
                              "Get list data element failed - object not found");
    }
    return RW_STATUS_NOTFOUND;
  }
  if (out_keyspec) {
    *out_keyspec = mobj->keyspec;
  }

  *out_msg = mobj->msg;
  
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_get_merged_keyspec(rw_keyspec_entry_t* pe,
                         rw_schema_minikey_opaque_t* mk,
                         rw_keyspec_path_t *reg_keyspec,
                         rw_keyspec_path_t **merged_keyspec)
{

  rw_status_t rw_status;

  RW_ASSERT(reg_keyspec);
  if (!reg_keyspec) {
    return RW_STATUS_FAILURE;
  }
  if (pe) {
    bool new_pe = FALSE;
    size_t entry_index;
    int key_index;
    new_pe = RW_KEYSPEC_PATH_IS_ENTRY_AT_TIP(reg_keyspec, NULL, pe, &entry_index, &key_index, NULL);
    if (new_pe == FALSE) {
      *merged_keyspec = RW_KEYSPEC_PATH_CREATE_DUP_OF_TYPE(reg_keyspec, NULL , &rw_schema_path_spec__descriptor, NULL);
      RW_ASSERT(*merged_keyspec != NULL);
      rw_status = RW_KEYSPEC_PATH_APPEND_ENTRY(*merged_keyspec, NULL , pe, NULL);
      RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
      RW_ASSERT(!rw_keyspec_path_has_wildcards(*merged_keyspec));
    } else {
      rw_status = RW_KEYSPEC_PATH_CREATE_DUP(reg_keyspec, NULL , merged_keyspec, NULL);
      RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
      if (key_index != -1) {
        rw_status = RW_KEYSPEC_PATH_ADD_KEYS_TO_ENTRY(*merged_keyspec, NULL, pe, entry_index, NULL);
        if (rw_status == RW_STATUS_FAILURE) {
          return RW_STATUS_FAILURE;
        }
        RW_ASSERT(!rw_keyspec_path_has_wildcards(*merged_keyspec));
      }
    }
    if (rw_keyspec_path_has_wildcards(*merged_keyspec)) {
      return RW_STATUS_FAILURE;
    }
  }
  else if (mk) {
    rw_status = RW_KEYSPEC_PATH_CREATE_DUP(reg_keyspec, NULL , merged_keyspec, NULL);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
    int depth = rw_keyspec_path_get_depth(*merged_keyspec);
    rw_status = rw_schema_mk_copy_to_keyspec(*merged_keyspec, mk, depth-1);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
    RW_ASSERT(!rw_keyspec_path_has_wildcards(*merged_keyspec));
  }

  return RW_STATUS_SUCCESS;
}

/*
 * create an object at a registration point
 */

rw_status_t
rwdts_member_reg_handle_create_element(rwdts_member_reg_handle_t regh,
                                       rw_keyspec_entry_t*       pe,
                                       const ProtobufCMessage*   msg,
                                       rwdts_xact_t*             xact)
{
    return (rwdts_member_data_create_list_element(xact, regh, pe, msg));
}

/*
 * create an object at a registration point with a keyspec path entry
 */

rw_status_t
rwdts_member_reg_handle_create_element_keyspec(rwdts_member_reg_handle_t regh,
                                               rw_keyspec_path_t*        keyspec,
                                               const ProtobufCMessage*   msg,
                                               rwdts_xact_t*             xact)
{
  return rwdts_member_reg_handle_handle_create_update_element(regh, keyspec, msg,
                                                              xact, 0, RWDTS_CREATE);
}

rw_status_t
rwdts_member_reg_handle_create_element_xpath(rwdts_member_reg_handle_t regh,
                                             const char*               xpath,
                                             const ProtobufCMessage*   msg,
                                             rwdts_xact_t*             xact)
{
  rw_keyspec_path_t*      keyspec = NULL;
  rw_status_t             rs;

  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  rs = rwdts_api_keyspec_from_xpath(reg->apih, xpath, &keyspec);

  if (rs  != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            rs, "%s for %s", RWDTS_ERRSTR_KEY_XPATH, xpath);
    if (!xact) {
      RWDTS_API_LOG_EVENT(reg->apih, DataMemberApiError, 
                          RWLOG_ATTR_SPRINTF("create list element failed - invalid xpath %s", xpath));
    }
    return rs;
  }

  return rwdts_member_reg_handle_create_element_keyspec(regh,
                                                        keyspec,
                                                        msg,
                                                        xact);
}
rw_status_t
rwdts_member_reg_handle_update_element(rwdts_member_reg_handle_t regh,
                                       rw_keyspec_entry_t*       pe,
                                       const ProtobufCMessage*   msg,
                                       uint32_t                  flags,
                                       rwdts_xact_t*             xact)
{
    return rwdts_member_data_update_list_element(xact,
                                                 regh,
                                                 pe,
                                                 msg,
                                                 flags);
}

rw_status_t
rwdts_member_reg_handle_update_element_keyspec(rwdts_member_reg_handle_t regh,
                                               const rw_keyspec_path_t*  keyspec,
                                               const ProtobufCMessage*   msg,
                                               uint32_t                  flags,
                                               rwdts_xact_t*             xact)
{
  return rwdts_member_reg_handle_handle_create_update_element(regh, (rw_keyspec_path_t*)keyspec,
                                                              msg, xact, flags, RWDTS_UPDATE);
}

rw_status_t
rwdts_member_reg_handle_update_element_xpath(rwdts_member_reg_handle_t regh,
                                             const char*               xpath,
                                             const ProtobufCMessage*   msg,
                                             uint32_t                  flags,
                                             rwdts_xact_t*             xact)
{
  rw_keyspec_path_t*      keyspec = NULL;
  rw_status_t             rs;
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  rs = rwdts_api_keyspec_from_xpath(reg->apih, xpath, &keyspec);

  if (rs  != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            rs, "%s for %s", RWDTS_ERRSTR_KEY_XPATH, xpath);
    if (!xact) {
      RWDTS_API_LOG_EVENT(reg->apih, DataMemberApiError, 
                          RWLOG_ATTR_SPRINTF("update list element failed - invalid xpath %s", xpath));
    }
    return rs;
  }

  rs = rwdts_member_reg_handle_update_element_keyspec(regh,
                                                        keyspec,
                                                        msg,
                                                        flags,
                                                        xact);
  rw_keyspec_path_free(keyspec, NULL);
  return(rs);
}

/*
 * delete an element
 */
rw_status_t
rwdts_member_reg_handle_delete_element(rwdts_member_reg_handle_t regh,
                                       const rw_keyspec_entry_t* pe,
                                       const ProtobufCMessage*   msg,
                                       rwdts_xact_t*             xact)
{
  return rwdts_member_data_delete_list_element(xact,
                                               regh,
                                               (rw_keyspec_entry_t*)pe,
                                               (ProtobufCMessage*)msg);
}

/*
 * delete an element  based on keyspec
 */
rw_status_t
rwdts_member_reg_handle_delete_element_keyspec(rwdts_member_reg_handle_t regh,
                                               const rw_keyspec_path_t*  keyspec,
                                               const ProtobufCMessage*   msg,
                                               rwdts_xact_t*             xact)
{
  return rwdts_member_data_delete_list_element_w_key(xact,
                                                     regh,
                                                     (rw_keyspec_path_t*)keyspec,
                                                     (ProtobufCMessage*)msg);
}

/*
 * delete an element  based on xpath
 */
rw_status_t
rwdts_member_reg_handle_delete_element_xpath(rwdts_member_reg_handle_t regh,
                                             const char*               xpath,
                                             const ProtobufCMessage*   msg,
                                             rwdts_xact_t*             xact)
{
  rw_keyspec_path_t*      keyspec = NULL;
  rw_status_t             rs;
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  rs = rwdts_api_keyspec_from_xpath(reg->apih, xpath, &keyspec);

  if (rs  != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            rs, "%s for %s", RWDTS_ERRSTR_KEY_XPATH, xpath);
    if (!xact) {
      RWDTS_API_LOG_EVENT(reg->apih, DataMemberApiError, 
                          RWLOG_ATTR_SPRINTF("delete list element failed - invalid xpath %s", xpath));
    }
    return rs;
  }

  RW_ASSERT(keyspec);

  rs = rwdts_member_reg_handle_delete_element_keyspec(regh, keyspec, (ProtobufCMessage*)msg, xact);

  RW_KEYSPEC_PATH_FREE(keyspec, NULL , NULL);

  return rs;
}

/*
 * get  the element
 */
rw_status_t
rwdts_member_reg_handle_get_element(rwdts_member_reg_handle_t  regh,
                                    rw_keyspec_entry_t*        pe,
                                    rwdts_xact_t*              xact,
                                    rw_keyspec_path_t**        out_keyspec,
                                    const ProtobufCMessage**   out_msg)
{
    return rwdts_member_data_get_list_element(xact, regh, pe, out_keyspec, out_msg);
}

/*
 * get  the element based on keyspec
 */
rw_status_t
rwdts_member_reg_handle_get_element_keyspec(rwdts_member_reg_handle_t  regh,
                                            rw_keyspec_path_t*         keyspec,
                                            rwdts_xact_t*              xact,
                                            rw_keyspec_path_t**        out_keyspec,
                                            const ProtobufCMessage**   out_msg)
{
    return rwdts_member_data_get_list_element_w_key(xact, regh, keyspec, out_keyspec, out_msg);
}

/*
 * get  the element based on xpath
 */
rw_status_t
rwdts_member_reg_handle_get_element_xpath(rwdts_member_reg_handle_t  regh,
                                          const char*                xpath,
                                          rwdts_xact_t*              xact,
                                          rw_keyspec_path_t**        out_keyspec,
                                          const ProtobufCMessage**   out_msg)
{
  rw_keyspec_path_t*      keyspec = NULL;
  rw_status_t             rs;

  rs = rwdts_api_keyspec_from_xpath(((rwdts_member_registration_t*)regh)->apih,
                                    xpath,
                                    &keyspec);
  if (rs  != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, NULL, NULL, NULL, NULL,
                            rs, "%s for %s", RWDTS_ERRSTR_KEY_XPATH, xpath);
    if (!xact) {
      RWDTS_API_LOG_EVENT(((rwdts_member_registration_t*)regh)->apih, DataMemberApiError, 
                          RWLOG_ATTR_SPRINTF("get list element failed - invalid xpath %s", xpath));
    }
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT(keyspec);

  rs = rwdts_member_reg_handle_get_element_keyspec(regh, keyspec, xact, out_keyspec, out_msg);
  RW_KEYSPEC_PATH_FREE(keyspec, NULL , NULL);

  return rs;
}

/*
 * get current curosor for this registration
 */
rwdts_member_cursor_t*
rwdts_member_reg_handle_get_current_cursor(rwdts_member_reg_handle_t regh,
                                           rwdts_xact_t*             xact)
{
  return rwdts_member_data_get_current_cursor(xact, regh);
}

/*
 * get next elemement based on this registration
 */
const ProtobufCMessage*
rwdts_member_reg_handle_get_next_element(rwdts_member_reg_handle_t regh,
                                         rwdts_member_cursor_t*    cursor,
                                         rwdts_xact_t*             xact,
                                         rw_keyspec_path_t**       out_keyspec)
{
  return rwdts_member_data_get_next_list_element(xact,
                                                 regh,
                                                 cursor,
                                                 out_keyspec);
}

static
void rwdts_member_data_merge_store(rwdts_member_data_record_t *mbr_data,
                                   rwdts_member_data_object_t*  mobj)
{
  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);
  RW_ASSERT_TYPE(mobj, rwdts_member_data_object_t);

  if (mbr_data->xact != NULL) {
    mobj->delete_mark = FALSE;
    mobj->update_flag = 1;
    mbr_data->reg->apih->stats.num_xact_update_objects++;
    mbr_data->reg->stats.num_xact_update_objects++;
  }

  // Free the old proto and update with the new proto
  if (mbr_data->flags & RWDTS_XACT_FLAG_REPLACE) {
    if (mbr_data->msg) {
      protobuf_c_message_free_unpacked(NULL, mobj->msg);
      mobj->msg =  mbr_data->msg;
      mbr_data->msg = NULL; // Consume the incomiong msg in mbr_data
    }
  } else {
    // Call the merge API
    // This should be the default
    protobuf_c_boolean ret;
    // merge(new_object, old_object)=> result in old_object
    ret = protobuf_c_message_merge(NULL, (ProtobufCMessage*)mbr_data->msg, mobj->msg);
    RW_ASSERT(ret == TRUE);
  }

  return;
}

static rwdts_member_data_object_t*
rwdts_member_data_store(rwdts_member_data_object_t** obj_list_p,
                        rwdts_member_data_record_t *mbr_data,
                        uint8_t *free_obj)
{
  RW_ASSERT_TYPE(mbr_data, rwdts_member_data_record_t);
  RW_ASSERT(obj_list_p);
  if (!obj_list_p) {
    return NULL;
  }

  rwdts_member_data_object_t* new_mobj = NULL;
  rwdts_api_t* apih = mbr_data->reg->apih;
  RW_ASSERT(apih);
  RW_ASSERT(free_obj);

  *free_obj = false;

  // keyp,  msg  and merged_keyspec are consumed by rwdts_member_data_init
  new_mobj = rwdts_member_data_init_frm_mbr_data(mbr_data, false, false, RWDTS_CREATE);
  // Failed to create a new object
  if (new_mobj == NULL) {
    RWDTS_API_LOG_EVENT(apih, DataMemberApiError, "create list element failed - rwdts_member_data_init returned null");
    return NULL;
  }

  if (mbr_data->xact != NULL) {
    new_mobj->create_flag = 1;
    apih->stats.num_xact_create_objects++;
    mbr_data->reg->stats.num_xact_create_objects++;
  } else {
    if (mbr_data->reg->shard && mbr_data->reg->shard->appdata) {
      rwdts_shard_handle_appdata_create_element(mbr_data->reg->shard, mbr_data->keyspec, mbr_data->msg);
      *free_obj = true;
    }
  }

  // Add to the hash
  RW_ASSERT(new_mobj->key);
  RW_ASSERT(new_mobj->key_len > 0);
  if (*free_obj == false) {
    HASH_ADD_KEYPTR(hh_data, (*obj_list_p), new_mobj->key, new_mobj->key_len, new_mobj);
  }

  return new_mobj;
}

static void
rwdts_member_pub_del(rwdts_member_data_record_t *mbr_data,
                     rwdts_member_data_object_t* mobj)
{

  if ((mbr_data->reg->flags & RWDTS_FLAG_PUBLISHER) != 0)  {
    uint32_t flags = 0;
    if (mbr_data->member_advise_cb.cb)  {
      flags |= RWDTS_XACT_FLAG_EXECNOW;
    }
    rwdts_member_data_advice_query_action(mbr_data->xact,
                                          (rwdts_member_reg_handle_t)mobj->reg,
                                          mobj->keyspec,
                                          mobj->msg,
                                          RWDTS_QUERY_DELETE,
                                          flags,
                                          &mbr_data->member_advise_cb,
                                          &mbr_data->rcvd_member_advise_cb,
                                          true);
    if (!mbr_data->xact && (mbr_data->reg->apih->rwtasklet_info &&
        ((mbr_data->reg->apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_BDB) ||
        (mbr_data->reg->apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_REDIS)))) {
      if (mbr_data->reg->kv_handle) {
        rw_status_t rs = rwdts_kv_handle_del_keyval(mbr_data->reg->kv_handle, 0, (char *)mobj->key, mobj->key_len,
                                                    rwdts_member_data_cache_set_callback, (void *)mbr_data->reg->kv_handle);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
        if (rs != RW_STATUS_SUCCESS) {
          /* Fail the VM and switchover to STANDBY */
        }
      }
    }
  }

  if ((mbr_data->reg->flags & RWDTS_FLAG_DATASTORE) != 0) {
    if (mbr_data->xact != NULL) {
      rwdts_kv_update_db_xact_precommit(mobj, RWDTS_QUERY_DELETE);
    } else {
      rwdts_kv_update_db_update(mobj, RWDTS_QUERY_DELETE);
    }
  }

  return;
}

rwdts_member_data_object_t**
rwdts_get_member_data_array_from_msgs_and_keyspecs(rwdts_member_registration_t *reg,
                                                   rwdts_query_result_t        *query_result,
                                                   uint32_t                    n,
                                                   uint32_t                    *n_op)
{
  rwdts_member_data_object_t **op;
  int i, j;

  if (n == 0 ) {
    return NULL;
  }

  rwdts_api_t* apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  *n_op = 0;
  op = RW_MALLOC0_TYPE(sizeof(rwdts_member_data_object_t*) * n, rwdts_member_data_object_t*);

  for (i = 0; i < n ; i++) {
    // ATTN -- fix this -- this is very very inefficient
    // Once the get_results API properly returns merged results
    // this can be changed.
    RW_ASSERT(query_result[i].message);
    if (protobuf_c_message_unknown_get_count(query_result[i].message)) {
      rwdts_report_unknown_fields(query_result[i].keyspec, apih, query_result[i].message);
      protobuf_c_message_delete_unknown_all(NULL, query_result[i].message);
    }

    bool in_list = false;
    for (j = 0; j < (*n_op); j++) {
      /* if  the registration category is not any make sure
       * the category we got is matching and force it to the registration category
       * ATTN is this needed ? shouldn't  the sender be correctly setting the catgory to begin with?
       */

      /* Force the category to match registration ks category */
      RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, query_result[i].keyspec, reg->keyspec);

      if (RW_KEYSPEC_PATH_IS_EQUAL(NULL, query_result[i].keyspec, op[j]->keyspec, NULL)) {
#if RWDTS_DEBUG_AUDIT
        char *ks_str = NULL;
        rw_keyspec_path_get_new_print_buffer(query_result[i].keyspec, NULL , rwdts_api_get_ypbc_schema(reg->apih),
                                         &ks_str);

        fprintf(stderr, "found ks string %s\n", ks_str ? ks_str : "");
        free(ks_str);
        ks_str = NULL;

        char *message_str = NULL;

        rw_json_pbcm_to_json(query_result[i].message,
                          query_result[i].message->descriptor,
                          &message_str);
        fprintf(stderr, "msgs[%d] is %p: %s\n", i, query_result[i].message, message_str?message_str:"NULL");
        RW_FREE(message_str);
        rw_json_pbcm_to_json(op[j]->msg,
                          op[j]->msg->descriptor,
                          &message_str);
        fprintf(stderr, "op[%d]->msgs is %p: %s\n", j, op[j]->msg, message_str?message_str:"NULL");
        RW_FREE(message_str);

#endif /* RWDTS_DEBUG_AUDIT */

        ProtobufCMessage*  old_msg = op[j]->msg;
        rw_keyspec_path_t* ks_out = NULL;
        ProtobufCMessage*  reroot_msg;
        reroot_msg = RW_KEYSPEC_PATH_REROOT_AND_MERGE(NULL,
                                                      query_result[i].keyspec,
                                                      op[j]->keyspec,
                                                      query_result[i].message,
                                                      old_msg,
                                                      &ks_out,
                                                      NULL);

#if RWDTS_DEBUG_AUDIT
        rw_keyspec_path_get_new_print_buffer(ks_out, NULL , rwdts_api_get_ypbc_schema(reg->apih),
                                         &ks_str);
        fprintf(stderr, "%p ks out is %s\n", ks_out, ks_str ? ks_str : "");
        free(ks_str);
#endif /* RWDTS_DEBUG_AUDIT */

        RW_ASSERT(ks_out);
        /* Force the category to match registration ks category */
        RW_KEYSPEC_FORCE_CATEGORY_TO_MATCH_REG(apih, ks_out, reg->keyspec);

        // Validate that the keyspec we got and the message is what we expect
        RW_ASSERT(reroot_msg->descriptor == reg->desc);
        RW_ASSERT(RW_KEYSPEC_PATH_IS_MATCH_DETAIL(NULL, reg->keyspec, ks_out, NULL, NULL,NULL));
        RW_ASSERT(reroot_msg);
        RW_ASSERT(!rw_keyspec_path_has_wildcards(ks_out));

        // Free the current contents in op[j]
        RW_ASSERT(reroot_msg != old_msg);
        protobuf_c_message_free_unpacked(&_rwdts_pbc_instance, old_msg);
        RW_ASSERT(op[j]->keyspec != ks_out);
        RW_KEYSPEC_PATH_FREE(op[j]->keyspec, NULL , NULL);


        // Create a new op[j] based on rerooted content
        op[j] = rwdts_member_data_init(reg, reroot_msg, ks_out, true, true);

        //RW_ASSERT(RW_KEYSPEC_PATH_IS_EQUAL(NULL, ks_out, keyspecs[i]));
        protobuf_c_message_free_unpacked(&_rwdts_pbc_instance, query_result[i].message);
        query_result[i].message = NULL; // --> consume messages;
        RW_KEYSPEC_PATH_FREE(query_result[i].keyspec, NULL , NULL);
        query_result[i].keyspec = NULL;
        in_list = true;
        break;
      }
    }
    if (in_list) continue;
    /* if  the registration category is not any make sure
     * the category we got is matching and force it to the registration category
     * ATTN is this needed ? shouldn't  the sender be correctly setting the catgory to begin with?
     */
    op[*n_op] = rwdts_member_data_init(reg, query_result[i].message, query_result[i].keyspec, true, true);
    // The api consumes query_result[i].message and query_result[i].keyspec
    query_result[i].message = NULL;
    query_result[i].keyspec = NULL;
    RW_ASSERT(op[*n_op]);
    (*n_op) += 1;
  }
  return op;
}

void
rwdts_member_cursor_reset(rwdts_member_cursor_t *cursor)
{
  // ATTN -- the following typecasting is risky --
  // Add proper typechecking once 7144 is supported
  ((rwdts_member_cursor_impl_t*)cursor)->position = 0;
}

rw_status_t
rwdts_member_reg_handle_handle_create_update_element(rwdts_member_reg_handle_t  regh,
                                                     rw_keyspec_path_t*         keyspec,
                                                     const ProtobufCMessage*    msg,
                                                     rwdts_xact_t*              xact,
                                                     uint32_t                   flags,
                                                     rwdts_member_data_action_t action)
{
  RW_ASSERT(action != RWDTS_DELETE);
  if (action == RWDTS_DELETE) {
    return RW_STATUS_FAILURE;
  }
  rwdts_member_registration_t* reg = (rwdts_member_registration_t*)regh;
  rwdts_api_t* apih = (rwdts_api_t*)reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rw_status_t rs;

  if (!RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL, reg->keyspec , keyspec, NULL)) {
    return RW_STATUS_FAILURE;
  }

  if (rw_keyspec_path_has_wildcards(keyspec)) {
    return RW_STATUS_FAILURE;
  }

  if (!msg) {
    return RW_STATUS_FAILURE;
  }

  if(!rw_keyspec_path_matches_message(keyspec, NULL, msg, true)) {
    return RW_STATUS_FAILURE;
  }

  if (reg->flags & RWDTS_FLAG_PUBLISHER) {
    size_t depth_i = rw_keyspec_path_get_depth(keyspec);
    size_t depth_o = rw_keyspec_path_get_depth(reg->keyspec);

    if (depth_i != depth_o) {
      ProtobufCMessage* matchmsg = NULL;
      rw_keyspec_path_t* matchks = NULL;
      if (depth_i < depth_o) {
        rw_keyspec_path_reroot_iter_state_t state;
        rw_keyspec_path_reroot_iter_init(keyspec, NULL, &state, msg, reg->keyspec);
        while (rw_keyspec_path_reroot_iter_next(&state)) {
          matchmsg = (ProtobufCMessage*)rw_keyspec_path_reroot_iter_get_msg(&state);
          if (matchmsg)  {
            matchks = (rw_keyspec_path_t*)rw_keyspec_path_reroot_iter_get_ks(&state);
            RW_ASSERT(matchks);
            RW_ASSERT(matchks != keyspec);
            RW_ASSERT(matchks != reg->keyspec);
            RW_ASSERT(!rw_keyspec_path_has_wildcards(matchks));
            RW_ASSERT(matchmsg != msg);
            if (action == RWDTS_CREATE) {
              rs = rwdts_member_data_create_list_element_w_key(xact, regh, matchks, matchmsg);
            } else {
              rs = rwdts_member_data_update_list_element_w_key(xact, regh, matchks, matchmsg, flags);
            }
            if (rs != RW_STATUS_SUCCESS) {
              return rs;
            }
            matchks = NULL; matchmsg = NULL;
          }
        }
        rw_keyspec_path_reroot_iter_done(&state);
        return RW_STATUS_SUCCESS;
      } else {
        matchmsg = RW_KEYSPEC_PATH_REROOT(keyspec, NULL, msg, reg->keyspec, NULL);
        RW_ASSERT(matchmsg != msg);
        if (matchmsg) {
          rs = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL, &matchks, NULL);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
          rs = RW_KEYSPEC_PATH_TRUNC_SUFFIX_N(matchks, NULL, rw_keyspec_path_get_depth(reg->keyspec), NULL);
          RW_ASSERT(rs == RW_STATUS_SUCCESS && matchks);
          RW_ASSERT(!rw_keyspec_path_has_wildcards(matchks));
          if (action == RWDTS_CREATE) {
            rs = rwdts_member_data_create_list_element_w_key(xact, regh, matchks, matchmsg);
          } else {
            rs = rwdts_member_data_update_list_element_w_key(xact, regh, matchks, matchmsg, flags);
          }
          rw_keyspec_path_free(matchks, NULL);
          return rs;
        }
        return RW_STATUS_FAILURE;
      }
    }
  }

  if (action == RWDTS_CREATE) {
    return (rwdts_member_data_create_list_element_w_key(xact, regh, keyspec, msg));
  } else {
    return rwdts_member_data_update_list_element_w_key(xact, regh, keyspec, msg, flags);
  }
}

