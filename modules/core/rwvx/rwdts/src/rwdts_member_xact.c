
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
 */


/**
 * @file rwdts_member_xact.c
 * @author Rajesh Velandy(rajesh.velandy@riftio.com)
 * @date 2014/10/02
 * @brief DTS  Member API FSMs.
 */

#include <rwdts_int.h>
#include <rwdts.h>
#include <rwlib.h>
#include <rwvcs.h>


/**
 * member (sub) transaction state transitions
 */

const char*
rwdts_member_state_to_str(rwdts_member_xact_state_t state)
{
  switch(state) {
    case RWDTS_MEMB_XACT_ST_INIT:
      return "ST_INIT";
    case RWDTS_MEMB_XACT_ST_PREPARE:
      return "ST_PREPARE";
    case RWDTS_MEMB_XACT_ST_COMMIT:
      return "ST_COMMIT";
    case RWDTS_MEMB_XACT_ST_COMMIT_RSP:
      return "ST_COMMIT_RSP";
    case RWDTS_MEMB_XACT_ST_PRECOMMIT:
      return "ST_PRECOMMIT";
    case RWDTS_MEMB_XACT_ST_ABORT:
      return "ST_ABORT";
    case RWDTS_MEMB_XACT_ST_ABORT_RSP:
      return "ST_ABORT_RSP";
    case RWDTS_MEMB_XACT_ST_END:
      return "ST_END";
    default:
      return "ST_UNKNOWN";
  }
  return "ST_UNKNOWN";
}

const char*
rwdts_member_event_to_str(rwdts_member_xact_evt_t evt)
{
  switch(evt) {
    case RWDTS_MEMB_XACT_EVT_PREPARE:
      return "EVT_PREPARE";
    case RWDTS_MEMB_XACT_QUERY_RSP:
      return "EVT_QUERY_RSP";
    case RWDTS_MEMB_XACT_EVT_PRECOMMIT:
      return "EVT_PRECOMMIT";
    case RWDTS_MEMB_XACT_EVT_COMMIT:
      return "EVT_COMMIT";
    case RWDTS_MEMB_XACT_EVT_COMMIT_RSP:
      return "EVT_COMMIT_RSP";
    case RWDTS_MEMB_XACT_EVT_ABORT:
      return "EVT_ABORT";
    case RWDTS_MEMB_XACT_EVT_ABORT_RSP:
      return "EVT_ABORT_RSP";
    case RWDTS_MEMB_XACT_EVT_END:
      return "EVT_END";
    default:
      return "EVT_UNKNOWN";
  }
  return "EVT_UNKNOWN";
}

const char*
rwdts_query_action_to_str(RWDtsQueryAction action, char *str, size_t str_len)
{
  switch(action) {
    case RWDTS_QUERY_INVALID:
      snprintf(str, str_len, "INVALID");
      break;
    case RWDTS_QUERY_CREATE:
      snprintf(str, str_len, "CREATE");
      break;
    case RWDTS_QUERY_READ:
      snprintf(str, str_len, "READ");
      break;
    case RWDTS_QUERY_UPDATE:
      snprintf(str, str_len, "UPDATE");
      break;
    case RWDTS_QUERY_DELETE:
      snprintf(str, str_len, "DELETE");
      break;
    case RWDTS_QUERY_RPC:
      snprintf(str, str_len, "RPC");
      break;
    default:
      snprintf(str, str_len, "UNKNOWN(%u)", (uint32_t)action);
      break;
  }
  return str;
}

bool
rwdts_member_responded_to_router(rwdts_xact_t* xact)
{
  rwdts_xact_query_t *xquery=NULL, *xqtmp=NULL;
  HASH_ITER(hh, xact->queries, xquery, xqtmp) {
    if (xquery->pending_response) {
      return false;
    }
  }
  return true;
}

static rw_status_t
rwdts_member_process_query_action(rwdts_xact_t*                xact,
                                  rwdts_member_registration_t* reg,
                                  rwdts_match_info_t*          match_info,
                                  rw_keyspec_path_t*           keyspec,
                                  ProtobufCMessage*            msg,
                                  rwdts_xact_query_t*          xquery);

static rw_status_t
rwdts_handle_reg_prepare(rwdts_xact_t *xact, rwdts_xact_query_t *xquery);

static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_init(rwdts_xact_t*           xact,
                                 rwdts_member_xact_evt_t evt,
                                 const void*             ud);
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_prepare(rwdts_xact_t*           xact,
                                    rwdts_member_xact_evt_t evt,
                                    const void*             ud);
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_precommit(rwdts_xact_t*          xact,
                                      rwdts_member_xact_evt_t evt,
                                      const void*             ud);
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_commit(rwdts_xact_t*          xact,
                                   rwdts_member_xact_evt_t evt,
                                   const void*             ud);
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_commit_rsp(rwdts_xact_t*      xact,
                                   rwdts_member_xact_evt_t evt,
                                   const void*             ud);

static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_abort(rwdts_xact_t*           xact,
                                  rwdts_member_xact_evt_t evt,
                                  const void*             ud);
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_abort_rsp(rwdts_xact_t*      xact,
                                  rwdts_member_xact_evt_t evt,
                                  const void*             ud);
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_end(rwdts_xact_t*           xact,
                                rwdts_member_xact_evt_t evt,
                                const void*             ud);
static rw_status_t
rwdts_member_xact_handle_prepare_query(rwdts_xact_t *xact, rwdts_xact_query_t *xquery);

static rw_status_t
rwdts_member_xact_handle_precommit(rwdts_xact_t *xact, RWDtsXact *input);

static RWDtsEventRsp
rwdts_member_xact_member_response_to_evtrsp(rwdts_member_rsp_code_t rsp_code);

static rw_status_t
rwdts_member_xact_handle_commit(rwdts_xact_t *xact, RWDtsXact *input);

static rw_status_t
rwdts_member_xact_handle_abort(rwdts_xact_t *xact, RWDtsXact *input);


static rw_status_t
rwdts_member_xact_handle_end(rwdts_xact_t *xact, const void *input);


static void
rwdts_member_xact_post_end(void *u);

static inline void rwdts_member_xact_post_end_f(rwsched_tasklet_ptr_t tasklet_info,
                                                rwsched_dispatch_queue_t queue,
                                                void *context)
{
  RW_ASSERT_TYPE(context, rwdts_xact_t);
  rwdts_xact_ref((rwdts_xact_t*)context, __PRETTY_FUNCTION__, __LINE__);
  rwsched_dispatch_async_f(tasklet_info,
                           queue,
                           context,
                           rwdts_member_xact_post_end);
}

static inline void
rwdts_member_xact_transition(rwdts_xact_t* xact, rwdts_member_xact_state_t state, rwdts_member_xact_evt_t evt)
{
  RW_ASSERT(xact);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(xact->apih, xact, XACT_FSM_TRANSITION,
                                 "Member xact fsm transition",
                                 (char*)rwdts_member_state_to_str(xact->mbr_state),
                                 (char*)rwdts_member_state_to_str(state));
  xact->mbr_state = state;
  RWDTS_XACT_LOG_STATE(xact, state, evt);
}

/*
 * Find a reg commit record within a transaction
 */

rwdts_reg_commit_record_t*
rwdts_member_find_reg_commit_record(rwdts_xact_t*  xact, rwdts_member_registration_t* reg)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_reg_commit_record_t *reg_crec = NULL;

  // Lookup the regisrtation in the hash -- note that the reg pointer is the key
  //Len 16 is used because, reg_id and apih pointer are used as keys
  RW_SKLIST_LOOKUP_BY_KEY(&(xact->reg_cc_list), &reg->reg_id,
                          (void *)&reg_crec);

  return reg_crec;
}

/*
 * Add  a commit record to the transaction
 */

rwdts_commit_record_int_t*
rwdts_add_commit_record(rwdts_xact_t*                xact,
                        rwdts_member_registration_t* reg,
                        rw_keyspec_path_t*           keyspec,
                        ProtobufCMessage*            msg,
                        rwdts_member_op_t            op,
                        rw_keyspec_path_t*           in_ks,
                        RwDtsQuerySerial*            serial)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_reg_commit_record_t *reg_crec = NULL;
  rw_status_t status;

  reg_crec = rwdts_member_find_reg_commit_record(xact, reg);

  /* There is no commit record yet for this registration in this transaction */
  if (reg_crec == NULL) {
    reg_crec         = RW_MALLOC0_TYPE(sizeof(rwdts_reg_commit_record_t), rwdts_reg_commit_record_t);
    reg_crec->reg    = reg;
    reg_crec->xact   = xact;
    reg_crec->reg_id = reg->reg_id;

    rw_status_t status = RW_SKLIST_INSERT(&(xact->reg_cc_list), reg_crec);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
  }

  RW_ASSERT_TYPE(reg_crec, rwdts_reg_commit_record_t);

  rwdts_commit_record_int_t *creci = RW_MALLOC0_TYPE(sizeof(rwdts_commit_record_int_t), rwdts_commit_record_int_t);
  RW_ASSERT_TYPE(creci, rwdts_commit_record_int_t);
  status = rw_keyspec_path_create_dup(keyspec, &xact->ksi, &creci->crec.ks);
  if(status != RW_STATUS_SUCCESS) {
    RWDTS_XACT_ABORT(xact, status, RWDTS_ERRSTR_KEY_DUP);
    return NULL;
  }
  creci->crec.op  = op;
  status  = rw_keyspec_path_create_dup(in_ks, &xact->ksi, &creci->crec.in_ks);
  if(status != RW_STATUS_SUCCESS) {
    RWDTS_XACT_ABORT(xact, status, RWDTS_ERRSTR_KEY_DUP);
    return NULL;
  }

  if (msg) {
    creci->crec.msg = protobuf_c_message_duplicate(NULL, msg, msg->descriptor);
  }
  creci->reg_crec = reg_crec;

  if (serial) {
    creci->serial = (RwDtsQuerySerial*)protobuf_c_message_duplicate(NULL, 
                                                                    &serial->base,
                                                                    serial->base.descriptor);
  }

  // Insert the commit record into the xact's commit list
  RW_ASSERT(reg_crec->n_commits <= reg_crec->size_commits);
  if (reg_crec->n_commits >= reg_crec->size_commits) {
    reg_crec->size_commits += RW_DTS_QUERY_MEMBER_COMMIT_ALLOC_CHUNK_SIZE;
    reg_crec->commits       = realloc(reg_crec->commits, sizeof(creci) * reg_crec->size_commits);

    RW_ASSERT(reg_crec->commits);
  }

  RW_ASSERT(reg_crec->commits != NULL);
  RW_ASSERT(reg_crec->size_commits > reg_crec->n_commits);
  reg_crec->commits[reg_crec->n_commits++] = creci;

  return creci;
}

/*
 * Process a query action
 */

static rw_status_t
rwdts_member_process_query_action(rwdts_xact_t*                xact,
                                  rwdts_member_registration_t* reg,
                                  rwdts_match_info_t*          match,
                                  rw_keyspec_path_t*           keyspec,
                                  ProtobufCMessage*            msg,
                                  rwdts_xact_query_t*          xquery)
{
  RW_ASSERT(xquery);
  RW_ASSERT(xquery->query);

  rwdts_member_op_t op;
  rw_status_t       rs;
  RWDtsQueryAction  action = xquery->query->action;
  uint32_t          flags = xquery->query->flags;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  switch(action) {
    case RWDTS_QUERY_CREATE:
      op = RWDTS_MEMBER_OP_CREATE;
      break;
    case RWDTS_QUERY_READ:
      op = RWDTS_MEMBER_OP_READ;
      break;
    case RWDTS_QUERY_UPDATE:
      op = RWDTS_MEMBER_OP_UPDATE;
      break;
    case RWDTS_QUERY_DELETE:
      op = RWDTS_MEMBER_OP_DELETE;
      break;
    case RWDTS_QUERY_RPC:
      op = RWDTS_MEMBER_OP_RPC;
      break;
    default:
      RW_ASSERT_MESSAGE(0, "Unknown action %u",
                        action);
  }

  /*
   * if this registration is a subscriber registration and we have
   * CACHE flags set - store this data within the transaction
   */

  if ((reg->flags & RWDTS_FLAG_CACHE) && (reg->flags & RWDTS_FLAG_SUBSCRIBER)) {

    rwdts_member_reg_handle_t regh = (rwdts_member_reg_handle_t)reg;

    switch(action) {
      case RWDTS_QUERY_CREATE:
        RW_ASSERT(keyspec);
        RW_ASSERT(msg);
        rs = rwdts_member_reg_handle_create_element_keyspec(regh, keyspec, msg, xact);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);

        break;
      case RWDTS_QUERY_UPDATE:
        /*
         * ATTN - if the incoming keyspec is wildcarded then delete the entire list
         *        if not delete the specific entry based on the message kpe
         */
        RW_ASSERT(keyspec);
        RW_ASSERT(msg);
        rs = rwdts_member_reg_handle_update_element_keyspec(regh, keyspec, msg, flags,xact);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);

        break;
      case RWDTS_QUERY_DELETE:{
        if (match->in_ks) {
          rs = rwdts_member_reg_handle_delete_element_keyspec(regh, match->in_ks, msg, xact);
        } else {
          rs = rwdts_member_reg_handle_delete_element_keyspec(regh, keyspec, msg, xact);
        }
        if (rs != RW_STATUS_SUCCESS) {
          RW_ASSERT_TYPE(reg->apih, rwdts_api_t);
          RWDTS_API_LOG_XACT_EVENT(reg->apih, xact ,RwDtsApiLog_notif_XactDeleteFailed, 
                                   "DTS member data delete failed for registration", reg->reg_id);
        }
      }
        break;
      default:
        break;
    }
  }

  if (flags & RWDTS_XACT_FLAG_RETURN_PAYLOAD) {

    RW_ASSERT((action == RWDTS_QUERY_CREATE) || (action == RWDTS_QUERY_UPDATE));
    rwdts_member_query_rsp_t rsp;
    ProtobufCMessage*        single_rsp = NULL;

    rw_keyspec_path_t *trunc_ks = NULL;
    RW_ZERO_VARIABLE(&rsp);
    rsp.msgs = &single_rsp;

    rs = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL, &trunc_ks, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    size_t depth_r = rw_keyspec_path_get_depth(reg->keyspec);

    rs = rw_keyspec_path_trunc_suffix_n(trunc_ks, NULL, depth_r);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    rs = rwdts_member_reg_handle_get_element_keyspec((rwdts_member_reg_handle_t)reg,
                                                     trunc_ks,
                                                     xact,
                                                     NULL,
                                                     (const struct ProtobufCMessage **)&rsp.msgs[0]);

    rsp.n_msgs = rsp.msgs[0]?1:0;
    rsp.evtrsp = RWDTS_EVTRSP_ACK;
    rsp.ks = trunc_ks;
    rwdts_member_send_response(xact, (rwdts_query_handle_t)match, &rsp);
    if (reg->shard && reg->shard->appdata && (rs == RW_STATUS_SUCCESS)) {
      if (rsp.msgs[0]) {
        protobuf_c_message_free_unpacked(NULL, rsp.msgs[0]);
      }
      rwdts_shard_handle_appdata_safe_put_exec(reg->shard, reg->shard->appdata->scratch_safe_id);
    }
    RW_KEYSPEC_PATH_FREE(trunc_ks, NULL, NULL);
  }

  // if this is a publisher registration and is a read and no prepare callback is registered
  // then respond with the data asscoiated with the registation

  if (((reg->flags & RWDTS_FLAG_PUBLISHER) ||
       ((reg->flags & RWDTS_FLAG_SUBSCRIBER) && (xquery->query->flags & RWDTS_XACT_FLAG_SUB_READ))) &&
      (action == RWDTS_QUERY_READ) &&
      (reg->flags & RWDTS_FLAG_NO_PREP_READ ||
       (reg->cb.cb.prepare == NULL))) {
    rwdts_member_query_rsp_t rsp;
    ProtobufCMessage*        single_rsp = NULL;
    rw_keyspec_path_t *out_ks = NULL;

    rw_keyspec_path_t *trunc_ks = NULL;
    RW_ZERO_VARIABLE(&rsp);

    rs = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL, &trunc_ks, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    size_t depth_r = rw_keyspec_path_get_depth(reg->keyspec);

    rs = rw_keyspec_path_trunc_suffix_n(trunc_ks, NULL, depth_r);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    // Send single result at a time
    rsp.msgs = &single_rsp;
    if(rw_keyspec_path_has_wildcards(trunc_ks)) {
      if (reg->shard && reg->shard->appdata) {
        ProtobufCMessage* msg = NULL;
        rwdts_appdata_cursor_t *cursor = rwdts_shard_handle_appdata_get_current_cursor(reg->shard);

        rwdts_shard_handle_appdata_reset_cursor(reg->shard);
        rsp.n_msgs = 1;
        while((msg =
               (ProtobufCMessage*)rwdts_shard_handle_appdata_getnext_list_element_keyspec(reg->shard,
                                                                                          cursor, &out_ks)) != NULL) {
          rsp.msgs[0] = protobuf_c_message_duplicate(NULL, msg, msg->descriptor);
          rsp.ks = out_ks;
          rsp.evtrsp = RWDTS_EVTRSP_ASYNC;
          rwdts_member_send_response(xact, (rwdts_query_handle_t)match, &rsp);
          if (out_ks) {
            rw_keyspec_path_free(out_ks, NULL);
          }
          protobuf_c_message_free_unpacked(NULL, rsp.msgs[0]);
        }
        RW_ZERO_VARIABLE(&rsp);
        rsp.evtrsp = RWDTS_EVTRSP_ACK;
        rwdts_member_send_response(xact, (rwdts_query_handle_t)match, &rsp);
      } else {
        rwdts_member_cursor_t *cursor = rwdts_member_data_get_cursor(xact,
                                                    (rwdts_member_reg_handle_t)reg);
        rsp.n_msgs = 1;
        while((rsp.msgs[0] =
               (ProtobufCMessage*)rwdts_member_reg_handle_get_next_element(
                                                  (rwdts_member_reg_handle_t)reg,
                                                  cursor, xact,
                                                  &out_ks)) != NULL) {
          rsp.ks = out_ks;
          rsp.evtrsp = RWDTS_EVTRSP_ASYNC;
          rwdts_member_send_response(xact, (rwdts_query_handle_t)match, &rsp);
        }
        rwdts_member_data_delete_cursors(xact, (rwdts_member_reg_handle_t)reg);
        RW_ZERO_VARIABLE(&rsp);
        rsp.evtrsp = RWDTS_EVTRSP_ACK;
        rwdts_member_send_response(xact, (rwdts_query_handle_t)match, &rsp);
      }
    } else {
      rs = rwdts_member_reg_handle_get_element_keyspec((rwdts_member_reg_handle_t)reg,
                                                       trunc_ks,
                                                       NULL,
                                                       NULL,
                                                       (const struct ProtobufCMessage **)&rsp.msgs[0]);
                                                      
      rsp.n_msgs = rsp.msgs[0]?1:0;
      rsp.evtrsp = RWDTS_EVTRSP_ACK;
      rsp.ks = trunc_ks;
      rwdts_member_send_response(xact, (rwdts_query_handle_t)match, &rsp);
      if (reg->shard && reg->shard->appdata && (rs == RW_STATUS_SUCCESS)) {
        if (rsp.msgs[0]) {
          protobuf_c_message_free_unpacked(NULL, rsp.msgs[0]);
        }
        rwdts_shard_handle_appdata_safe_put_exec(reg->shard, reg->shard->appdata->scratch_safe_id);
      }
    }
    RW_KEYSPEC_PATH_FREE(trunc_ks, NULL, NULL);
  }

  rwdts_commit_record_int_t* creci = rwdts_add_commit_record(xact, reg, keyspec, msg, op, (rw_keyspec_path_t *)xquery->query->key->keyspec,
                                                             match->query?match->query->serial:NULL);

  char *ks_str = NULL;
  char op_str[128] = "";

  UNUSED(op_str);
  UNUSED(ks_str);
  rw_keyspec_path_get_new_print_buffer(keyspec, NULL, NULL, &ks_str);

  RWDTS_API_LOG_XACT_DEBUG_EVENT(xact->apih, xact, COMMIT_REC_ADDED, "Added commit record",
                                 (char*)rwdts_query_action_to_str(op, op_str, sizeof(op_str)),
                                 ks_str ? ks_str : "");

  RW_ASSERT(creci != NULL);
  free(ks_str);

  return RW_STATUS_SUCCESS;
}

/**
 * Deinit a reg commit record
 */

rw_status_t
rwdts_member_reg_commit_record_deinit(rwdts_reg_commit_record_t *reg_crec)
{
  int i;

  if (reg_crec == NULL) {
    return RW_STATUS_SUCCESS;
  }
  RW_ASSERT_TYPE(reg_crec, rwdts_reg_commit_record_t);
  reg_crec->reg =  NULL;

  // Remove from the transactions skiplist
  if (reg_crec->xact) {
    rwdts_reg_commit_record_t *removed;
    RW_ASSERT_TYPE(reg_crec->xact, rwdts_xact_t);
    rw_status_t status = RW_SKLIST_REMOVE_BY_KEY(&reg_crec->xact->reg_cc_list,
                                                 &reg_crec->reg_id,
                                                 &removed);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    reg_crec->xact = NULL;
  }

  if (reg_crec->n_commits) {
    for (i =  reg_crec->n_commits - 1;  i >= 0; i--) {
      rwdts_member_commit_record_deinit(reg_crec->commits[i]);
      reg_crec->commits[i] = NULL;
    }
  }

  reg_crec->n_commits = reg_crec->size_commits = 0;

  free(reg_crec->commits);
  reg_crec->commits = NULL;

  RW_FREE_TYPE(reg_crec, rwdts_reg_commit_record_t);


  return RW_STATUS_SUCCESS;
}

/**
 * Deinit a transaction commit record
 */

rw_status_t
rwdts_member_commit_record_deinit(rwdts_commit_record_int_t *creci)
{
  if (creci == NULL) {
    return RW_STATUS_SUCCESS;
  }

  RW_ASSERT_TYPE(creci, rwdts_commit_record_int_t);

  if (creci->crec.ks) {
    rw_keyspec_path_free(creci->crec.ks, NULL);
    creci->crec.ks = NULL;
  }

  if (creci->crec.in_ks) {
    rw_keyspec_path_free(creci->crec.in_ks, NULL);
    creci->crec.in_ks = NULL;
  }

  if (creci->crec.msg) {
    protobuf_c_message_free_unpacked(&protobuf_c_default_instance, creci->crec.msg);
    creci->crec.msg = NULL;
  }

  int index;

  /* FIXME Gear Dog this may be really not fast */

  // Find the index of the record we are trying to delete
  for (index = 0; index < creci->reg_crec->n_commits && creci->reg_crec->commits[index] != creci; index++);

  // Is this the last element?
  if (index == (creci->reg_crec->n_commits-1)) {
    // Then set to NULL
    creci->reg_crec->commits[index] = NULL;
  } else {
    // Move the remaining block in place of the one we found
    memmove(&creci->reg_crec->commits[index], &creci->reg_crec->commits[index + 1],
            (creci->reg_crec->n_commits - index - 1));
  }

  // Reduce the number of commits
  creci->reg_crec->n_commits--;

  if (creci->serial) {
    protobuf_c_message_free_unpacked(NULL, &creci->serial->base);
    creci->serial = NULL;
  }

  RW_FREE_TYPE(creci, rwdts_commit_record_int_t);
  return RW_STATUS_SUCCESS;
}


rwdts_fsm_transition_routine rwdts_member_fsm[] =
{
  rwdts_member_xact_fsm_state_init,
  rwdts_member_xact_fsm_state_prepare,
  rwdts_member_xact_fsm_state_precommit,
  rwdts_member_xact_fsm_state_commit,
  rwdts_member_xact_fsm_state_commit_rsp,
  rwdts_member_xact_fsm_state_abort,
  rwdts_member_xact_fsm_state_abort_rsp,
  rwdts_member_xact_fsm_state_end
};

void rwdts_member_xact_run(rwdts_xact_t*  xact,
                           rwdts_member_xact_evt_t evt,
                           const void *ud) {
  RW_ASSERT(xact);
  RW_ASSERT(xact->mbr_state >= RWDTS_MEMB_XACT_ST_INIT &&
            xact->mbr_state <= RWDTS_MEMB_XACT_ST_END);

  rwdts_fsm_status_t status = rwdts_member_fsm[(int)xact->mbr_state](xact, evt, ud);

  if (status == FSM_FAILED) {
    //  Log the failure
    RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                             "dts member transaction fsm failed",
                             (char*)rwdts_member_state_to_str(xact->mbr_state),
                             (char*)rwdts_member_event_to_str(evt),
                             xact->apih->client_path);
  }
  return;
}

static rw_status_t
rwdts_process_solicit_rsp_advise(rwdts_xact_t* xact, RWDtsQuery* query)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(query);

  // ATTN -- Baiju to add journal support
  // How many pubs are we expecting solicit from
  xact->solicit_rsp = 1;
  
  RW_ASSERT(query->serial);
  // rwdts_store_cache_obj(xact);
  return RW_STATUS_SUCCESS;
}

static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_init(rwdts_xact_t*           xact,
                                 rwdts_member_xact_evt_t evt,
                                 const void*             ud)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(RWDTS_VALID_MEMB_XACT_EVT(evt));
  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_fsm_status_t fsm_status = FSM_FAILED;

  RWDTS_XACT_LOG_STATE(xact, xact->mbr_state, evt);

  switch(evt) {
    case RWDTS_MEMB_XACT_EVT_PREPARE: {
      // Invoke callback to the member based on registration
      //
      rwdts_xact_query_t *xquery = (rwdts_xact_query_t*)ud;
      RW_ASSERT(xquery);
      RW_ASSERT(xquery->query);

      apih->stats.num_prepare_evt_init_state++;

      rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_PREPARE, RWDTS_MEMB_XACT_EVT_PREPARE);

      rw_status_t rs = rwdts_member_xact_handle_prepare_query(xact, xquery);

      if (rs == RW_STATUS_SUCCESS) {
        fsm_status = FSM_OK;
        if (!rwdts_is_transactional(xact)) {
          if (xquery->query->flags&RWDTS_XACT_FLAG_SOLICIT_RSP) {
            rwdts_process_solicit_rsp_advise(xact, xquery->query);
          } else {
            rwdts_store_cache_obj(xact);
          }
          rwdts_journal_consume_xquery (xquery, RWDTS_MEMB_XACT_EVT_END);
        }
        else {
          rwdts_journal_consume_xquery (xquery, RWDTS_MEMB_XACT_EVT_PREPARE);
        }
      } else {
        RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                                 "member xact fsm failed",
                                 (char*)rwdts_member_state_to_str(xact->mbr_state),
                                 (char*)rwdts_member_event_to_str(evt),
                                 xact->apih->client_path);
        rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_END, NULL);
      }
    }
    break;

    case RWDTS_MEMB_XACT_EVT_END: {
      // Move to the terminal state and cleanup
      // This is a synthetic event for the transaction to clean-up
      rw_status_t rs = rwdts_member_xact_handle_end(xact, ud);

      apih->stats.num_end_evt_init_state++;

      if (rs == RW_STATUS_SUCCESS) {
        // Transition to the commit state
        fsm_status = FSM_OK;
      }
    }
    break;

    case RWDTS_MEMB_XACT_EVT_PRECOMMIT: {

      if (xact->blocks_ct) {
        // Take a ref of xact
        rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);

        // Handle  this query request and respond to the router
        rw_status_t rs = rwdts_member_xact_handle_precommit(xact, (RWDtsXact*)ud);

        xact->evtrsp = RWDTS_EVTRSP_ACK;

        apih->stats.num_pcommit_evt_prepare_state++;

        // Iterate through the queries and clean them
        rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
        HASH_ITER(hh, xact->queries, xquery, qtmp) {
          xquery->responded = 0;
        }

        if (rs == RW_STATUS_SUCCESS) {
          // Transition to the precommit state
          rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_PRECOMMIT, RWDTS_MEMB_XACT_EVT_PRECOMMIT);
          fsm_status = FSM_OK;
        }
      }
    }
    break;

    default: {
      // Log
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "Unknown transition - dts member xact fsm failed",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;
  }
  return fsm_status;
}


/*
 * fsm state prepare
 */

static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_prepare(rwdts_xact_t*           xact,
                                    rwdts_member_xact_evt_t evt,
                                    const void*             ud)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(RWDTS_VALID_MEMB_XACT_EVT(evt));
  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_fsm_status_t fsm_status = FSM_FAILED;

  /* This should be handled in better way. This bit is set after sending
     the initial responsr to router. This bit should be reset for the response to
     prepare event from router. If this is not done, the router FSM does not move ahead
     rwdts_xact_query_t *tempxquery=NULL, *tqtmp=NULL;
     HASH_ITER(hh, xact->queries, tempxquery, tqtmp) {
     tempxquery->responded = 0;
     } */

  RWDTS_XACT_LOG_STATE(xact, xact->mbr_state, evt);

  switch(evt) {
    case RWDTS_MEMB_XACT_EVT_PREPARE: {

      rwdts_xact_query_t *xquery = (rwdts_xact_query_t*)ud;
      RW_ASSERT(xquery);

      // Invoke callback to the member based on registration
      RW_ASSERT(xquery->query);

      apih->stats.num_prepare_evt_prepare_state++;

      // Handle  this query request and respond to the router
      rw_status_t rs = rwdts_member_xact_handle_prepare_query(xact, xquery);

      if (rs == RW_STATUS_SUCCESS) {
        fsm_status = FSM_OK;
        rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_PREPARE, RWDTS_MEMB_XACT_EVT_PREPARE);
        if (!rwdts_is_transactional(xact)) {
          rwdts_store_cache_obj(xact);
          rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_END, NULL);
        }
      } else {
        RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                                 "DTS member xact fsm failed",
                                 (char*)rwdts_member_state_to_str(xact->mbr_state),
                                 (char*)rwdts_member_event_to_str(evt),
                                 xact->apih->client_path);
        rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_END, NULL);
      }
    }
    break;

    case RWDTS_MEMB_XACT_EVT_PRECOMMIT: {

      // Handle  this query request and respond to the router
      rw_status_t rs = rwdts_member_xact_handle_precommit(xact, (RWDtsXact*)ud);

      apih->stats.num_pcommit_evt_prepare_state++;

      // Iterate through the queries and clean them
      rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
      HASH_ITER(hh, xact->queries, xquery, qtmp) {
        xquery->responded = 0;
      }

      if (rs == RW_STATUS_SUCCESS) {
        // Transition to the precommit state
        rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_PRECOMMIT, RWDTS_MEMB_XACT_EVT_PRECOMMIT);
        fsm_status = FSM_OK;
      }
    }
    break;

    case RWDTS_MEMB_XACT_EVT_ABORT: {
      apih->stats.num_abort_evt_prepare_state++;
      // Issue an abort to the member
      // Handle  this query request and respond to the router
      rw_status_t rs = rwdts_member_xact_handle_abort(xact, (RWDtsXact*)ud);


      /* Perform KV xact abort operation */
      rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
      HASH_ITER(hh, xact->queries, xquery, qtmp) {
        rwdts_journal_consume_xquery (xquery, RWDTS_MEMB_XACT_EVT_ABORT);
        xquery->responded = 0;
      }

      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      if (rs == RW_STATUS_SUCCESS) {
        rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_ABORT_RSP, RWDTS_MEMB_XACT_EVT_ABORT_RSP);
        rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_ABORT_RSP, NULL);
      }
      fsm_status = FSM_OK;
    }
    break;

    case RWDTS_MEMB_XACT_QUERY_RSP: {
      apih->stats.num_qrsp_evt_prepare_state++;

      /*
       * we are done with all the responses -  post an
       * end trasanction if this is a non transaction
       * unless we're a block owner (aka client)
       */
      if (!rwdts_is_transactional(xact)) {
        if (rwdts_member_responded_to_router(xact)) {
          rwdts_xact_query_t *tmp_query=NULL, *qtmp=NULL;
          bool async_rsp = FALSE;
          HASH_ITER(hh, xact->queries, tmp_query, qtmp) {
            RWDtsEventRsp evtrsp = rwdts_final_rsp_code(tmp_query);
            if (evtrsp == RWDTS_EVTRSP_ASYNC) {
              async_rsp = TRUE;
              break;
            }
          }

          if(!async_rsp) {
            rwdts_member_xact_post_end_f(apih->tasklet,
                                     apih->client.rwq,
                                     xact);
          }
        }
        else {
          RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_QueryPendingRsp,
                                   "Received Query responses, pending responses still",
                                   xact->num_prepares_dispatched, xact->num_responses_received);
        }
      }
      fsm_status = FSM_OK;
    }
    break;

    case RWDTS_MEMB_XACT_EVT_END: {
      apih->stats.num_end_evt_prepare_state++;
      // Move to the terminal state and cleanup
      // This is a synthetic event for the transaction to clean-up
      rw_status_t rs = rwdts_member_xact_handle_end(xact, (RWDtsXact*)ud);

      if (rs == RW_STATUS_SUCCESS) {
        // Transition to the commit state
        fsm_status = FSM_FINISHED;
      }
    }
    break;

    default: {
      // Log
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "DTS member xact fsm failed",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;
  }

  return fsm_status;
}

/*
 * fsm state precommit
 */
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_precommit(rwdts_xact_t*           xact,
                                      rwdts_member_xact_evt_t evt,
                                      const void*             ud)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(RWDTS_VALID_MEMB_XACT_EVT(evt));
  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_fsm_status_t fsm_status = FSM_FAILED;

  /* This should be handled in better way. This bit is set after sending
     the initial responsr to router. This bit should be reset for the response to
     prepare event from router. If this is not done, the router FSM does not move ahead
     rwdts_xact_query_t *tempxquery=NULL, *tqtmp=NULL;
     HASH_ITER(hh, xact->queries, tempxquery, tqtmp) {
     tempxquery->responded = 0;
     } */

  RWDTS_XACT_LOG_STATE(xact, xact->mbr_state, evt);

  switch(evt) {
    case RWDTS_MEMB_XACT_EVT_PREPARE: {
      apih->stats.num_prepare_evt_pcommit_state++;
      // The router should not be sending us a pepare after sending precommit
      // send a Nak and log
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "DTS member xact fsm failed - prepare rcvd in pre-commit",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;

    case RWDTS_MEMB_XACT_EVT_PRECOMMIT: {
      apih->stats.num_pcommit_evt_pcommit_state++;
      // Makes no sense to get multiple precommits -- did the router screw up? send a Nack
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "DTS member xact fsm failed - precommit rcvd in pre-commit",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;

    case RWDTS_MEMB_XACT_EVT_COMMIT: {
      apih->stats.num_commit_evt_pcommit_state++;
      // Issue commit to the member and move to the commit state
      // Handle  this query request and respond to the router
      rw_status_t rs = rwdts_member_xact_handle_commit(xact, (RWDtsXact*)ud);

      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      if (rs == RW_STATUS_SUCCESS) {
        // Transition to the commit state

        rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
        HASH_ITER(hh, xact->queries, xquery, qtmp) {
          rwdts_journal_consume_xquery (xquery, RWDTS_MEMB_XACT_EVT_COMMIT);
        }

        rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_COMMIT_RSP, RWDTS_MEMB_XACT_EVT_COMMIT_RSP);
        rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_COMMIT_RSP, NULL);
        rwdts_store_cache_obj(xact);
        fsm_status = FSM_OK;
      }
    }
    break;

    case RWDTS_MEMB_XACT_EVT_ABORT: {
      apih->stats.num_abort_evt_pcommit_state++;
      // Issue an abort to the member
      // Handle  this query request and respond to the router
      rw_status_t rs = rwdts_member_xact_handle_abort(xact, (RWDtsXact*)ud);


      /* Perform KV xact abort operation */
      rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
      HASH_ITER(hh, xact->queries, xquery, qtmp) {
        rwdts_journal_consume_xquery (xquery, RWDTS_MEMB_XACT_EVT_ABORT);
        xquery->responded = 0;
      }

      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      if (rs == RW_STATUS_SUCCESS) {
        // Transition to the abort state
        rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_ABORT_RSP, RWDTS_MEMB_XACT_EVT_ABORT_RSP);
        rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_ABORT_RSP, NULL);
        fsm_status = FSM_OK;
      }
    }
    break;

    case RWDTS_MEMB_XACT_QUERY_RSP:
    // No - op;
    fsm_status = FSM_OK;
    break;



    default: {
      // Log
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "DTS member xact fsm failed",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;
  }
  return fsm_status;
}

/*
 * fsm state commit
 */
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_commit(rwdts_xact_t*           xact,
                                   rwdts_member_xact_evt_t evt,
                                   const void*             ud)
{
  rwdts_fsm_status_t fsm_status = FSM_FAILED;
  // ATTN Comment this for now
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(RWDTS_VALID_MEMB_XACT_EVT(evt));
  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWDTS_XACT_LOG_STATE(xact, xact->mbr_state, evt);

  switch(evt) {
    case RWDTS_MEMB_XACT_EVT_END: {
      apih->stats.num_end_evt_commit_state++;
      // Move to the terminal state and cleanup
      // This is a synthetic event for teh transaction to clean-up
      rw_status_t rs = rwdts_member_xact_handle_end(xact, (RWDtsXact*)ud);

      if (rs == RW_STATUS_SUCCESS) {
        // Transition to the commit state
        fsm_status = FSM_FINISHED;
      }
    }
    break;

    case RWDTS_MEMB_XACT_QUERY_RSP:
    // No - op;
    fsm_status = FSM_OK;
    break;

    default: {
      // Log
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "DTS member xact fsm failed",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;
  }
  return fsm_status;
}

/*
 * fsm state commit_rsp
 */
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_commit_rsp(rwdts_xact_t*           xact,
                                   rwdts_member_xact_evt_t evt,
                                   const void*             ud)
{
  rwdts_fsm_status_t fsm_status = FSM_FAILED;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(RWDTS_VALID_MEMB_XACT_EVT(evt));
  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWDTS_XACT_LOG_STATE(xact, xact->mbr_state, evt);

  switch(evt) {
    case RWDTS_MEMB_XACT_EVT_COMMIT_RSP: {
        rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_COMMIT, RWDTS_MEMB_XACT_EVT_COMMIT);
        rwdts_member_xact_post_end_f(apih->tasklet,
                                     apih->client.rwq,
                                     xact);
        fsm_status = FSM_OK;
    }
    break;

    default: {
      // Log
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "DTS member xact fsm failed",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;
  }
  return fsm_status;
}

/*
 * Handle an abort from the router
 */
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_abort(rwdts_xact_t*           xact,
                                  rwdts_member_xact_evt_t evt,
                                  const void*             ud)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(RWDTS_VALID_MEMB_XACT_EVT(evt));
  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_fsm_status_t fsm_status = FSM_OK;

  switch(evt) {
    case RWDTS_MEMB_XACT_EVT_END: {
      apih->stats.num_end_evt_abort_state++;
      // Move to the terminal state and cleanup
      // This is a synthetic event for teh transaction to clean-up
      rw_status_t rs = rwdts_member_xact_handle_end(xact, (RWDtsXact*)ud);

      if (rs == RW_STATUS_SUCCESS) {
        // Transition to the end state
        fsm_status = FSM_FINISHED;
      }
    }
    break;

    case RWDTS_MEMB_XACT_QUERY_RSP:
    // No - op;
    fsm_status = FSM_OK;
    break;

    default: {
      // Log
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "DTS member xact fsm failed",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;
  }

  return fsm_status;
}

/*
 * Handle an abort response from member
 */
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_abort_rsp(rwdts_xact_t*           xact,
                                  rwdts_member_xact_evt_t evt,
                                  const void*             ud)
{
  rwdts_fsm_status_t fsm_status = FSM_FAILED;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(RWDTS_VALID_MEMB_XACT_EVT(evt));
  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWDTS_XACT_LOG_STATE(xact, xact->mbr_state, evt);

  switch(evt) {
    case RWDTS_MEMB_XACT_EVT_ABORT_RSP: {
        rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_ABORT, RWDTS_MEMB_XACT_EVT_ABORT);
        rwdts_member_xact_post_end_f(apih->tasklet,
                                     apih->client.rwq,
                                     xact);
        fsm_status = FSM_OK;
    }
    break;

    default: {
      // Log
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_XactFsmFailed,
                               "DTS member xact fsm failed",
                               (char*)rwdts_member_state_to_str(xact->mbr_state),
                               (char*)rwdts_member_event_to_str(evt),
                               xact->apih->client_path);
    }
    break;
  }
  return fsm_status;
  return fsm_status;
}

static void rwdts_xact_unref_async( void*ud)
{
  rwdts_xact_unref((rwdts_xact_t*)ud, __PRETTY_FUNCTION__, __LINE__);
  return;
}

/*
 * Handle an end from the router
 */
static rwdts_fsm_status_t
rwdts_member_xact_fsm_state_end(rwdts_xact_t*           xact,
                                rwdts_member_xact_evt_t evt,
                                const void*             ud)
{
  switch (evt) {
  case RWDTS_MEMB_XACT_EVT_PREPARE:
    rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_PREPARE, RWDTS_MEMB_XACT_EVT_PREPARE);
    rwdts_member_xact_run(xact, evt, ud);
    return FSM_OK;
  case RWDTS_MEMB_XACT_EVT_PRECOMMIT:
    rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_INIT, evt);
    rwdts_member_xact_run(xact, evt, ud);
    return FSM_OK;
  case RWDTS_MEMB_XACT_QUERY_RSP:
    return FSM_FINISHED;
  case RWDTS_MEMB_XACT_EVT_END:
    if (!rwdts_is_transactional(xact)) {
      /* unref this after the current stack unwinds
       * so that calling functions can still access xact
       * after returning from here.
       */
      rwsched_dispatch_async_f(xact->apih->tasklet,
                               xact->apih->client.rwq,
                               xact,
                               rwdts_xact_unref_async);
      return FSM_OK;
    }
  default:
    break;
  }

  // something is wrong -- -this transaction is done ...
  RW_ASSERT_MESSAGE(0, "invalid end %u", evt);
  return FSM_FAILED;
}

static void rwdts_update_query_rsp_code(rwdts_xact_query_t *xquery)
{
  rwdts_match_info_t *match;

  xquery->evtrsp = RWDTS_EVTRSP_NA;

  match = RW_SKLIST_HEAD(&(xquery->reg_matches), rwdts_match_info_t);
  while (match) {
    if (match->evtrsp == RWDTS_EVTRSP_ASYNC) {
      xquery->evtrsp = RWDTS_EVTRSP_ASYNC;
      return;
    }
    if (match->evtrsp == RWDTS_EVTRSP_NACK) {
      xquery->evtrsp = RWDTS_EVTRSP_NACK;
      return;
    }
    if (match->evtrsp == RWDTS_EVTRSP_ACK) {
      xquery->evtrsp = RWDTS_EVTRSP_ACK;
    }
    if (match->evtrsp == RWDTS_EVTRSP_INTERNAL) {
      xquery->evtrsp = RWDTS_EVTRSP_INTERNAL;
    }
    match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
  }
  return;
}

static void rwdts_update_query_exp_rsp(rwdts_xact_query_t *xquery)
{
  rwdts_match_info_t *match;

  xquery->exp_rsp_count  = 0;
  match = RW_SKLIST_HEAD(&(xquery->reg_matches), rwdts_match_info_t);
  while (match) {
    if (match->reg && match->reg->cb.cb.prepare) {
      xquery->exp_rsp_count++;
    }
    match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
  }
  return;
}



rwsched_dispatch_source_t
rwdts_member_timer_create(rwdts_api_t *apih,
                          uint64_t timeout,
                          dispatch_function_t timer_cb,
                          void *ud,
                          bool start)
{
  rwsched_dispatch_source_t timer = NULL;
  timer = rwsched_dispatch_source_create(apih->tasklet,
                                         RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                         0,
                                         0,
                                         apih->client.rwq);
  rwsched_dispatch_source_set_event_handler_f(apih->tasklet,
                                              timer,
                                              timer_cb);
  rwsched_dispatch_set_context(apih->tasklet,
                               timer,
                               ud);

  rwsched_dispatch_source_set_timer(apih->tasklet,
                                    timer,
                                    dispatch_time(DISPATCH_TIME_NOW, timeout),
                                    timeout,
                                    0);
  if(start)
  {
    rwsched_dispatch_resume(apih->tasklet,
                            timer);
  }
  return timer;
}



static rwdts_member_rsp_code_t
rwdts_find_keys_call_prepare(rwdts_member_registration_t* reg,
                             const rw_keyspec_path_t *keyspec,
                             const rwdts_xact_info_t *xact_info,
                             RWDtsQueryAction action, uint32_t credits,
                             void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rw_keyspec_path_t** keyspecs = NULL;
  ProtobufCMessage *msg = NULL;
  uint32_t i;
  bool wildcard = rw_keyspec_path_has_wildcards(keyspec);

  uint32_t key_count = rwdts_get_stored_keys(reg, &keyspecs);

  if (!key_count) {
    return RWDTS_ACTION_OK;
  }

  for (i=0; i < key_count; i++) {

    if (!wildcard) {
      if(!rw_keyspec_path_is_a_sub_keyspec(NULL, keyspec, keyspecs[i])) {
        continue;
      }
    }

    msg = rw_keyspec_path_create_delete_delta(keyspecs[i], &xact->ksi,
                                              reg->keyspec);
    if (!msg) {
      RW_FREE(keyspecs);
      return RWDTS_ACTION_OK;
    }

    reg->cb.cb.prepare(xact_info, action, keyspecs[i], msg, credits, getnext_ptr);
    protobuf_c_message_free_unpacked(NULL, msg);
  }
  if (keyspecs) {
    RW_FREE(keyspecs);
  }
  return RWDTS_ACTION_OK;
}

static rw_status_t
rwdts_handle_reg_prepare(rwdts_xact_t *xact, rwdts_xact_query_t *xquery)
{
  int counter = 0;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT(xquery);
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWDtsQuery *query = xquery->query;
  RW_ASSERT(query);

  rwdts_member_rsp_code_t rsp_code;
  rwdts_match_info_t *match;
  rwdts_member_registration_t *reg = NULL;
  ProtobufCMessage  *proto = NULL;
  rw_keyspec_path_t *keyspec = NULL;
  uint32_t credits =0;
  char tmp_log_xact_id_str[128] = "";

  rwdts_update_query_exp_rsp(xquery);

  match = RW_SKLIST_HEAD(&(xquery->reg_matches), rwdts_match_info_t);

  while (match) {
    counter++;
    reg = match->reg;
    keyspec = match->ks;
    proto = match->msg;
    RW_ASSERT(keyspec);
    RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

    /* Group.  If this registration is in a group, add group_xact to this xact.  */
    if (reg->ingroup) {
      RW_ASSERT(reg->group.group);
      if (!xact->group || reg->group.group->id >= xact->group_len || !xact->group[reg->group.group->id]) {
        if (!xact->group || xact->group_len <= reg->group.group->id) {
          xact->group = realloc(xact->group, apih->group_len * sizeof(rwdts_group_xact_t *));
          int i;
          for (i=xact->group_len; i<apih->group_len; i++) {
            xact->group[i] = NULL;
          }
          xact->group_len = apih->group_len;
        }
        if (!xact->group[reg->group.group->id]) {
          xact->group[reg->group.group->id] = RW_MALLOC0_TYPE(sizeof(rwdts_group_xact_t), rwdts_group_xact_t);
        }
        /* Call group xinit as needed */
        RW_ASSERT(!xact->group[reg->group.group->id]->xinit);
        if (reg->group.group->cb.xact_init) {
          xact->group[reg->group.group->id]->scratch = reg->group.group->cb.xact_init(reg->group.group, xact, reg->group.group->cb.ctx);
        }
        xact->group[reg->group.group->id]->xinit = TRUE;
      }
    }

    if (xact->tr) {
      RWDtsTracerouteEnt ent;
      rwdts_traceroute_ent__init(&ent);
      ent.line = __LINE__;
      ent.func = (char*)__PRETTY_FUNCTION__;
      ent.what = RWDTS_TRACEROUTE_WHAT_MEMBER_MATCH;
      ent.matchpath = reg->keystr;
      ent.dstpath = apih->client_path;
      ent.srcpath = xquery->query->key->keystr;
      //        uint32_t blockid = 0;//??xact->xact->subx[xact->block_idx]->blockidx;
      ent.queryid[0] = (((0x00ffffffff&((uint64_t)0)) << 32) | ((uint64_t)(query->queryidx)));
      ent.n_queryid = 1;
      ent.has_reg_id = 1;
      ent.reg_id = reg->reg_id;
      rwdts_dbg_tracert_add_ent(xact->tr, &ent);
      if (xact->tr->print_) {
        fprintf(stderr, "%s:%u: TYPE:%s, DST:%s, SRC:%s, MATCH-PATH:%s\n", ent.func, ent.line,
                rwdts_print_ent_type(ent.what), ent.dstpath, ent.srcpath, ent.matchpath);
      }
      RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_TraceMemberMatch, rwdts_xact_id_str(&xact->id,tmp_log_xact_id_str, sizeof(tmp_log_xact_id_str)),
                  apih->client_path, query->queryidx,xquery->query->key->keystr,reg->keystr,reg->reg_id);
    }

    if ((reg->cb.cb.prepare && query->action != RWDTS_QUERY_READ)  ||
        (reg->cb.cb.prepare && !(reg->flags&RWDTS_FLAG_NO_PREP_READ))) {

      char *ks_str = NULL;
      const  rw_yang_pb_schema_t* ks_schema = NULL;
      // Get the schema for this registration
      ks_schema = ((ProtobufCMessage*)reg->keyspec)->descriptor->ypbc_mdesc->module->schema;

      // If registration schema is NULL, set it to the schema from API
      if (ks_schema == NULL) {
        ks_schema = rwdts_api_get_ypbc_schema(apih);
      }

      rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL, ks_schema, &ks_str);

      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_PrepareCbInvoked,
                               "DTS member Invoking prepare callback",
                               rwdts_get_app_addr_res(xact->apih, reg->cb.cb.prepare),
                               ks_str ? ks_str : "");
      free(ks_str);

      rwdts_xact_info_t *xact_info = &match->xact_info;
      xact_info->apih   = apih;
      xact_info->xact   = xact;
      xact_info->queryh = (rwdts_query_handle_t)match;
      xact_info->event  = RWDTS_MEMBER_EVENT_PREPARE;
      xact_info->regh   = (rwdts_member_reg_handle_t)reg;
      xact_info->ud     = reg->cb.ud;
      xact_info->scratch = reg->ingroup ? (xact->group[reg->group.group->id]->scratch) : NULL;

      if (query->has_credits) {
        credits = xquery->credits;
      }

      xact->num_prepares_dispatched++;
      apih->stats.num_prepare_cb_exec++;
      reg->stats.num_prepare_cb_invoked++;
      if (xact->tr && xact->tr->break_prepare) {
        G_BREAKPOINT();
      }
      xact_info->transactional = rwdts_is_transactional(xact_info->xact);

      rwdts_member_start_prep_timer_cancel(match);

      RWVCS_LATENCY_CHK_PRE(apih->tasklet->instance);

      if (!reg->appconf && (query->action == RWDTS_QUERY_DELETE) && !proto && (reg->flags & RWDTS_FLAG_DELTA_READY)) {
        ProtobufCMessage  *msg = rw_keyspec_path_create_delete_delta(keyspec, &xact->ksi,
                                                                     reg->keyspec);
        if (!msg) {
          rsp_code = rwdts_find_keys_call_prepare(reg, keyspec, xact_info, query->action,
                                                  credits, match->getnext_ptr);
        } else {
          rsp_code = reg->cb.cb.prepare(xact_info, query->action, keyspec, msg, credits, match->getnext_ptr);
        }
      } else {
        rsp_code = reg->cb.cb.prepare(xact_info, query->action, keyspec, proto, credits, match->getnext_ptr);
      }

      RWVCS_LATENCY_CHK_POST(apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                             reg->cb.cb.prepare, "cb.prepare at %s", apih->client_path);

      switch(rsp_code) {
        case RWDTS_ACTION_INTERNAL:
          RW_ASSERT(strstr(apih->client_path, "RW.DTSRouter"));
        case RWDTS_ACTION_OK:
        case RWDTS_ACTION_NOT_OK:
        case RWDTS_ACTION_NA:
          g_atomic_int_set(&match->resp_done, 1);
          rwdts_member_prep_timer_cancel(match, false, __LINE__);
          break;
        case RWDTS_ACTION_ASYNC:
          //appconf special case
          if (g_atomic_int_get(&match->resp_done)) {
            rwdts_member_prep_timer_cancel(match, false, __LINE__);
          }
          else {
            rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
          }
          break;
        default:
          RWDTS_ASSERT_MESSAGE(0,apih, keyspec, xact, "%s:%d [[ %s ]] bad prepare callback %s with status code %d",
                               __func__, __LINE__, apih->client_path, rw_btrace_get_proc_name(reg->cb.cb.prepare), rsp_code);
          break;
      } /* switch(rsp_code) */

      if (match->evtrsp == RWDTS_EVTRSP_NULL) {
        match->evtrsp = rwdts_member_xact_member_response_to_evtrsp(rsp_code);
      }

      // if the member turned around right away and said ack, consider this a final response
      if (xquery->evtrsp != RWDTS_EVTRSP_ASYNC) {
        xact->num_responses_received++;
      }
      //RW_ASSERT(xquery->evtrsp != RWDTS_EVTRSP_NA);
    } else {
      match->evtrsp = (reg->flags&RWDTS_FLAG_NO_PREP_READ) ? RWDTS_EVTRSP_ASYNC : RWDTS_EVTRSP_ACK;
    }

    // If the response from the member is an Ack/Async
    // process the query and create commit records if needed.
    //
    // ATTN == fix this for ASYNC the processing should only be done once  the member has acked the response
    if (match->evtrsp == RWDTS_EVTRSP_ACK || match->evtrsp == RWDTS_EVTRSP_ASYNC) {

      RW_ASSERT(xquery->query != NULL);

      RW_ASSERT(reg);
      RW_ASSERT(match->reg);
      RW_ASSERT(match->reg == reg);

      rw_status_t rs = rwdts_member_process_query_action(xact, reg, match, keyspec, proto, xquery);

      if (rs != RW_STATUS_SUCCESS) {
        // ATTN failed to apply the data - abort the transaction
        RWDTS_XACT_ABORT(xact, rs, RWDTS_ERRSTR_QUERY_ACTION);
        return RW_STATUS_FAILURE;
      }
    }
    g_atomic_int_set(&match->prepared, 1);
    match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
  }
  rwdts_update_query_rsp_code(xquery);

  return RW_STATUS_SUCCESS;
}

/*
 * Handle a prepare query from the router
 */
static rw_status_t
rwdts_member_xact_handle_prepare_query(rwdts_xact_t *xact, rwdts_xact_query_t *xquery)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t* apih = xact->apih;

  RW_ASSERT(xquery);
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWDtsQuery *query = xquery->query;
  RW_ASSERT(query);
  rw_status_t retval = RW_STATUS_SUCCESS;

  if (RW_SKLIST_LENGTH(&(apih->reg_list))) {

    retval = rwdts_member_find_matches(apih,query, &(xquery->reg_matches));

    if (retval != RW_STATUS_SUCCESS) {
      // If we did not find a matching keyspec in registrations,
      // then respond with NA and success
      xquery->evtrsp = RWDTS_EVTRSP_NA;
    } else {
      return rwdts_handle_reg_prepare(xact, xquery);
    }
  } else {
    xquery->evtrsp = RWDTS_EVTRSP_NA;
  }
  return RW_STATUS_SUCCESS;
}
/*
 * Handle  commmit  request for this transaction
 */

static rw_status_t
rwdts_member_xact_handle_commit(rwdts_xact_t *xact, RWDtsXact *input)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT(apih);
  RW_ASSERT(input);

  rwdts_member_rsp_code_t rsp_code =  RWDTS_ACTION_OK;
  int abrt = 0;

  /* Group callback(s) */
  {
    int i;
    for (i=0; xact->group && i<xact->group_len; i++) {
      if (xact->group[i] && !xact->group[i]->commit) {
        RW_ASSERT(apih->group[i]);
        if (apih->group[i]->cb.xact_event) {
          rsp_code = apih->group[i]->cb.xact_event(apih,
                                                   apih->group[i],
                                                   xact,
                                                   RWDTS_MEMBER_EVENT_COMMIT,
                                                   apih->group[i]->cb.ctx,
                                                   xact->group[i]->scratch);
          if (rsp_code != RWDTS_ACTION_OK) {
            abrt++;
          }
        }
        xact->group[i]->commit = TRUE;
      }
    }
  }

  xact->evtrsp = RWDTS_EVTRSP_NA;

  rwdts_reg_commit_record_t *reg_crec = NULL;
  reg_crec = RW_SKLIST_HEAD(&(xact->reg_cc_list), rwdts_reg_commit_record_t);
  while(reg_crec && !abrt) {
    rwdts_member_registration_t *reg = reg_crec->reg;

    {
      if (reg->cb.cb.commit) {
        RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_CommitCbInvoked,
                                 "DTS member Invoking commit callback",
                                 rwdts_get_app_addr_res(xact->apih, (void *)reg->cb.cb.commit));

        rwdts_xact_info_t *xact_info = &reg_crec->xact_info;
        xact_info->apih   = apih;
        xact_info->xact   = xact;
        xact_info->queryh = NULL;
        xact_info->event  = RWDTS_MEMBER_EVENT_COMMIT;
        xact_info->regh   = (rwdts_member_reg_handle_t)reg;
        xact_info->ud     = reg->cb.ud;
        xact_info->scratch = reg->ingroup ? (xact->group[reg->group.group->id]->scratch) : NULL;

        RWVCS_LATENCY_CHK_PRE(apih->tasklet->instance);

        rsp_code = reg->cb.cb.commit(xact_info,
                                     reg_crec->n_commits,
                                     (const rwdts_commit_record_t**)reg_crec->commits);

        RWVCS_LATENCY_CHK_POST(apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                               reg->cb.cb.commit, "cb.commit at %s", apih->client_path);

        RW_ASSERT(rsp_code != RWDTS_ACTION_ASYNC);
        apih->stats.num_commit_cb_exec++;
        reg->stats.num_commit_cb_invoked++;
        if (rsp_code != RWDTS_ACTION_OK) {
          abrt++;
        }
        xact->evtrsp = rwdts_member_xact_member_response_to_evtrsp(rsp_code);
        RW_ASSERT(xact->evtrsp != RWDTS_EVTRSP_NA); // ATTN after prepare can a precommit callback return NA?
      } else {
        xact->evtrsp = RWDTS_EVTRSP_ACK;
      }
    }
    reg_crec = RW_SKLIST_NEXT(reg_crec, rwdts_reg_commit_record_t, element);
  }
  if (abrt) {
    xact->evtrsp = RWDTS_EVTRSP_NACK;
  }
  return RW_STATUS_SUCCESS;
}

/*
 * During precommit check a create came for an existing object.
 * create is allowed only once for the object. 
 * Update should be used for modifying object already created.
 *
 */

#if 0
static rwdts_member_rsp_code_t
rwdts_member_precommit_create_check(rwdts_xact_t *xact, 
                                    rwdts_member_registration_t *reg,
                                    uint32_t n_commits,           
                                    const rwdts_commit_record_t** commits)                                   
{
  int k, depth1, depth2;
  ProtobufCMessage* out_msg = NULL, *matchmsg;

  for (k=0; k<n_commits; k++) {
    if (commits[k]->op == RWDTS_MEMBER_OP_CREATE) {
      rwdts_member_reg_handle_get_element_keyspec((rwdts_member_reg_handle_t)reg,
                                                 commits[k]->ks, 
                                                 NULL,
                                                 NULL, 
                                                 (const ProtobufCMessage**)&out_msg);
      if (out_msg) {
        depth1 =  rw_keyspec_path_get_depth(commits[k]->ks);
        depth2 =  rw_keyspec_path_get_depth(commits[k]->in_ks); 
        if (depth1 > depth2 ) {
         matchmsg = RW_KEYSPEC_PATH_REROOT(commits[k]->in_ks, &xact->ksi, commits[k]->msg, commits[k]->ks, NULL);
        }
        else {
         matchmsg = RW_KEYSPEC_PATH_REROOT(commits[k]->ks, &xact->ksi, out_msg, commits[k]->in_ks, NULL);
        }
        if (matchmsg) {
          protobuf_c_message_free_unpacked(NULL, matchmsg); 
          matchmsg = NULL;
          if (reg->shard && reg->shard->appdata) {
            protobuf_c_message_free_unpacked(NULL, out_msg);
            out_msg = NULL;
          }
          return RWDTS_ACTION_NOT_OK; 
        }
        if (reg->shard && reg->shard->appdata) {
          protobuf_c_message_free_unpacked(NULL, out_msg);
          out_msg = NULL;
        }
      }
    }
  }
  return RWDTS_ACTION_OK;
}
#endif

/*
 * Handle  pre-commmit  request for this transaction
 */

static rw_status_t
rwdts_member_xact_handle_precommit(rwdts_xact_t *xact, RWDtsXact *input)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT(apih);
  RW_ASSERT(input);

  rwdts_member_rsp_code_t rsp_code =  RWDTS_ACTION_OK;
  int abrt = 0;

  /* Group callback(s) */
  {
    int i;
    for (i=0; xact->group && i<xact->group_len; i++) {
      if (xact->group[i] && !xact->group[i]->precommit) {
        RW_ASSERT(apih->group[i]);
        if (apih->group[i]->cb.xact_event) {
          rsp_code = apih->group[i]->cb.xact_event(apih,
                                                   apih->group[i],
                                                   xact,
                                                   RWDTS_MEMBER_EVENT_PRECOMMIT,
                                                   apih->group[i]->cb.ctx,
                                                   xact->group[i]->scratch);
          if (rsp_code != RWDTS_ACTION_OK) {
            abrt++;
          }
        }
        xact->group[i]->precommit = TRUE;
      }
    }
  }

  xact->evtrsp = RWDTS_EVTRSP_NA;

  rwdts_reg_commit_record_t *reg_crec = NULL;
  reg_crec = RW_SKLIST_HEAD(&(xact->reg_cc_list), rwdts_reg_commit_record_t);
  while(reg_crec) {
    /* Found a successfull match -- rs == RW_STATUS_SUCCESS &&  reg != NULL */
    rwdts_member_rsp_code_t rsp_code = RWDTS_ACTION_OK;
    rwdts_member_registration_t *reg = reg_crec->reg;
    {
      if (reg->cb.cb.precommit) {
        RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_PrecommitCbInvoked,
                                 "DTS member Invoking precommit callback",
                                 rwdts_get_app_addr_res(xact->apih, (void *)reg->cb.cb.precommit));

        rwdts_xact_info_t *xact_info = &reg_crec->xact_info;
        xact_info->apih   = apih;
        xact_info->xact   = xact;
        xact_info->queryh = NULL;
        xact_info->event  = RWDTS_MEMBER_EVENT_PRECOMMIT;
        xact_info->regh   = (rwdts_member_reg_handle_t)reg;
        xact_info->ud     = reg->cb.ud;
        xact_info->scratch = reg->ingroup ? (xact->group[reg->group.group->id]->scratch) : NULL;

        RWVCS_LATENCY_CHK_PRE(apih->tasklet->instance);
#if 0
        rsp_code = rwdts_member_precommit_create_check(xact, reg, reg_crec->n_commits,
                                  (const rwdts_commit_record_t**)reg_crec->commits);  
#endif
        if (rsp_code != RWDTS_ACTION_OK) {
          xact->evtrsp = RWDTS_EVTRSP_NACK;
        }
        else {
          rsp_code = reg->cb.cb.precommit(xact_info,
                                        reg_crec->n_commits,
                                        (const rwdts_commit_record_t**)reg_crec->commits);

          RWVCS_LATENCY_CHK_POST(apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                               reg->cb.cb.precommit, "cb.precommit at %s", apih->client_path);

          RW_ASSERT(rsp_code != RWDTS_ACTION_ASYNC);
          xact->evtrsp = rwdts_member_xact_member_response_to_evtrsp(rsp_code);
          apih->stats.num_pcommit_cb_exec++;
          reg->stats.num_precommit_cb_invoked++;
        }
        if (rsp_code != RWDTS_ACTION_OK) {
          abrt++;
        }
        // ATTN after prepare can a precommit callback return NA?
        RW_ASSERT(xact->evtrsp != RWDTS_EVTRSP_NA);
      } else {
        xact->evtrsp = RWDTS_EVTRSP_ACK;
      }
    }
    reg_crec = RW_SKLIST_NEXT(reg_crec, rwdts_reg_commit_record_t, element);
  } /* HASH_ITER */
  if (abrt) {
    xact->evtrsp = RWDTS_EVTRSP_NACK;
  }

  return RW_STATUS_SUCCESS;
}

/*
 * Handle abort for this transaction
 */
static rw_status_t
rwdts_member_xact_handle_abort(rwdts_xact_t *xact, RWDtsXact *input)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT(apih);
  RW_ASSERT(input);

  rwdts_member_rsp_code_t rsp_code =  RWDTS_ACTION_OK;
  int abrt = 0;

  if (xact->member_new_blocks) {
    rwdts_xact_block_t *member_new_blocks;
    int i;
    for (i=0; i < xact->n_member_new_blocks; i++) {
      member_new_blocks = xact->member_new_blocks[i];
      rwdts_xact_block_unref(member_new_blocks, __PRETTY_FUNCTION__, __LINE__);
    }
    RW_FREE(xact->member_new_blocks);
    xact->member_new_blocks = NULL;
    xact->n_member_new_blocks = 0;
  }

  rwdts_xact_query_t *xquery=NULL, *xqtmp=NULL;
  HASH_ITER(hh, xact->queries, xquery, xqtmp) {
    rwdts_match_info_t *match = RW_SKLIST_HEAD(&(xquery->reg_matches), rwdts_match_info_t);
    while (match) {
      rwdts_member_prep_timer_cancel(match, false, __LINE__);
      match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
    }
  }

  /* Group callback(s) */
  {
    int i;
    for (i=0; xact->group && i<xact->group_len; i++) {
      if (xact->group[i] && !xact->group[i]->abort) {
        RW_ASSERT(apih->group[i]);
        if (apih->group[i]->cb.xact_event) {
          rsp_code = apih->group[i]->cb.xact_event(apih,
                                                   apih->group[i],
                                                   xact,
                                                   RWDTS_MEMBER_EVENT_ABORT,
                                                   apih->group[i]->cb.ctx,
                                                   xact->group[i]->scratch);
          if (rsp_code != RWDTS_ACTION_OK) {
            abrt++;
          }
        }
        xact->group[i]->abort = TRUE;
      }
    }
  }

  xact->evtrsp = RWDTS_EVTRSP_NA;

  rwdts_reg_commit_record_t *reg_crec = NULL;
  reg_crec = RW_SKLIST_HEAD(&(xact->reg_cc_list), rwdts_reg_commit_record_t);

  while (reg_crec) {
    /* Found a successfull match -- rs == RW_STATUS_SUCCESS &&  reg != NULL */
    rwdts_member_rsp_code_t rsp_code;
    rwdts_member_registration_t *reg = reg_crec->reg;

    {
      if (reg->cb.cb.abort) {
        RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_AbortCbInvoked,
                                 "DTS member Invoking abort callback",
                                 rwdts_get_app_addr_res(xact->apih, (void *)reg->cb.cb.abort));
        rwdts_xact_info_t *xact_info = &reg_crec->xact_info;
        xact_info->apih   = apih;
        xact_info->xact   = xact;
        xact_info->queryh = NULL;
        xact_info->event  = RWDTS_MEMBER_EVENT_ABORT;
        xact_info->regh   = (rwdts_member_reg_handle_t)reg;
        xact_info->ud     = reg->cb.ud;
        xact_info->scratch = reg->ingroup ? (xact->group[reg->group.group->id]->scratch) : NULL;

        RWVCS_LATENCY_CHK_PRE(apih->tasklet->instance);

        rsp_code = reg->cb.cb.abort(xact_info,
                                    reg_crec->n_commits,
                                    (const rwdts_commit_record_t**)reg_crec->commits);

        RWVCS_LATENCY_CHK_POST(apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                               reg->cb.cb.abort, "cb.abort at %s", apih->client_path);

        RW_ASSERT(rsp_code != RWDTS_ACTION_ASYNC);

        apih->stats.num_abort_cb_exec++;
        reg->stats.num_abort_cb_invoked++;
        xact->evtrsp = rwdts_member_xact_member_response_to_evtrsp(rsp_code);
        // ATTN after prepare can a precommit callback return NA?
        if (rsp_code != RWDTS_ACTION_OK) {
          abrt++;
        }
        RW_ASSERT(xact->evtrsp != RWDTS_EVTRSP_NA);
      } else {
        xact->evtrsp = RWDTS_EVTRSP_ACK;
      }
    }
    reg_crec = RW_SKLIST_NEXT(reg_crec, rwdts_reg_commit_record_t, element);
  } /* HASH_ITER */
  if (abrt) {
    /* This being an abort operation in the first place, this NACK
       means the system will be inconsistent.  Would be nice to
       capture what group/registration returned !OK above. */
    xact->evtrsp = RWDTS_EVTRSP_NACK;
  }
  return RW_STATUS_SUCCESS;
}


/*
 * Convert a member response to Query Event response
 */
static RWDtsEventRsp
rwdts_member_xact_member_response_to_evtrsp(rwdts_member_rsp_code_t rsp_code)
{
  RWDtsEventRsp evtrsp;

  switch(rsp_code) {
    case RWDTS_ACTION_OK:
      evtrsp = RWDTS_EVTRSP_ACK;
      break;

    case RWDTS_ACTION_NOT_OK:
      evtrsp = RWDTS_EVTRSP_NACK;
      break;

    case RWDTS_ACTION_NA:
      evtrsp = RWDTS_EVTRSP_NA;
      break;

    case RWDTS_ACTION_ASYNC:
      evtrsp = RWDTS_EVTRSP_ASYNC;
      break;

    case RWDTS_ACTION_INTERNAL:
      evtrsp = RWDTS_EVTRSP_INTERNAL;
      break;

    default:
      RW_ASSERT_MESSAGE(0,"Unknown rsp code %u ", evtrsp);
      break;
  } /* switch(rsp_code) */
  return evtrsp;
}

static void
rwdts_member_xact_post_end(void *u)
{
  rwdts_xact_t *xact = (rwdts_xact_t*)u;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_END, NULL);
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
}

/*
 * Convert a xact end request for this transaction
 */
static rw_status_t
rwdts_member_xact_handle_end(rwdts_xact_t *xact, const void *input)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT(apih);


  apih->stats.num_end_exec++;

  rwdts_member_xact_transition(xact, RWDTS_MEMB_XACT_ST_END, RWDTS_MEMB_XACT_EVT_END);

  /* Group callback(s), and deinit the list of group referneces */
  {
    int i;
    for (i=0; xact->group && i<xact->group_len; i++) {
      if (xact->group[i]) {
        if (!xact->group[i]->xdeinit) {
          if (apih->group[i]->cb.xact_deinit) {
            /* Typically the user freeing scratch... */

            apih->group[i]->cb.xact_deinit(apih->group[i],
                                           xact,
                                           apih->group[i]->cb.ctx,
                                           xact->group[i]->scratch);
            xact->group[i]->scratch = NULL;
          }
          xact->group[i]->xdeinit = TRUE;
        }

        /* Also, toss group_xact record */
        RW_FREE_TYPE(xact->group[i], rwdts_group_xact_t);
        xact->group[i] = NULL;
      }
    }
    if (xact->group) {
      free(xact->group);
      xact->group = NULL;
      xact->group_len = 0;
    }
  }

  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, XACT_DESTROYED, "destroying xact");
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  return RW_STATUS_SUCCESS;
}

bool
rwdts_is_transactional(rwdts_xact_t *xact)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdts_xact_query_t *xquery=NULL, *xqtmp=NULL;
  HASH_ITER(hh, xact->queries, xquery, xqtmp) {
    if ((xquery->query->action != RWDTS_QUERY_READ) &&
        !(xquery->query->flags&RWDTS_XACT_FLAG_NOTRAN)) {
      return true;
    }
  }
  return false;
}
