
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
 * @file   rwdts_member_kv.c
 * @author Prashanth Bhaskar
 * @date 2014/09/15
 * @brief DTS  member API definitions
 */

#include <stdlib.h>
#include <rwtypes.h>
#include <rwdts_int.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_keyspec.h>
#include <rwdts.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <rwlib.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>

/*
 * Internal structure to send data between data member APIs for async dispatch
 */

/*******************************************************************************
 *                      Static prototypes                                      *
 *******************************************************************************/


static rwdts_kv_light_reply_status_t
rwdts_kv_light_insert_xact_obj_cb(rwdts_kv_light_set_status_t status, int serial_num, void *userdata);

static rwdts_kv_light_reply_status_t
rwdts_kv_light_delete_xact_obj_cb(rwdts_kv_light_del_status_t status, int serial_num, void *userdata);

/*******************************************************************************
 *                      Static functions                                       *
 *******************************************************************************/


static rwdts_kv_light_reply_status_t
rwdts_kv_light_insert_xact_obj_cb(rwdts_kv_light_set_status_t status, int serial_num, void *userdata)
{
  rwdts_member_data_object_t *mobj = (rwdts_member_data_object_t *)userdata;
  mobj->serial_num = serial_num;
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t
rwdts_kv_light_delete_xact_obj_cb(rwdts_kv_light_del_status_t status, int serial_num, void *userdata)
{
  rwdts_member_data_object_t *mobj = (rwdts_member_data_object_t *)userdata;
  mobj->serial_num = serial_num;
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t
rwdts_kv_light_insert_xact_commit_obj_cb(rwdts_kv_light_set_status_t status, void *userdata)
{
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t
rwdts_kv_light_delete_xact_commit_obj_cb(rwdts_kv_light_del_status_t status, void *userdata)
{
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t
rwdts_kv_light_insert_obj_cb(rwdts_kv_light_set_status_t status, int serial_num, void *userdata)
{
   return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t
rwdts_kv_light_delete_obj_cb(rwdts_kv_light_del_status_t status, void *userdata)
{
   return RWDTS_KV_LIGHT_REPLY_DONE;
}


rw_status_t
rwdts_kv_update_db_xact_commit(rwdts_member_data_object_t *mobj, RWDtsQueryAction action)
{
  rwdts_api_t *apih;
  rwdts_member_registration_t *reg;

  reg = mobj->reg;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  apih = reg->apih;
  RW_ASSERT(apih);

  if (mobj->kv_tab_handle == NULL) {
    /* Problem */
    return RW_STATUS_FAILURE;
  }

  RWDTS_CREATE_SHARD(reg->reg_id, apih->client_path, apih->router_path);
  /* Perform KV xact operation */
  if (apih->db_up && ((action == RWDTS_QUERY_CREATE) ||
      (RWDTS_QUERY_UPDATE == action))) {
    rwdts_kv_light_api_xact_insert_commit(mobj->kv_tab_handle, mobj->serial_num,
                                          shard, (void *)mobj->key,
                                          mobj->key_len, (void *)rwdts_kv_light_insert_xact_commit_obj_cb,
                                          (void *)mobj);
  } else if (apih->db_up && (action == RWDTS_QUERY_DELETE)) {
    rwdts_kv_light_table_xact_delete_commit(mobj->kv_tab_handle, mobj->serial_num,
                                            (void *)mobj->key,
                                            mobj->key_len,
                                            (void *)rwdts_kv_light_delete_xact_commit_obj_cb,
                                            (void *)mobj);
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_kv_update_db_update(rwdts_member_data_object_t *mobj, RWDtsQueryAction action)
{
  rwdts_api_t *apih;
  rwdts_member_registration_t *reg;
  ProtobufCMessage *msg;
  uint8_t *payload;
  size_t  payload_len;
  uint32_t db_number;
  rw_status_t status;
  rwdts_kv_table_handle_t *kv_tab_handle = NULL;
  rwdts_shard_info_detail_t *shard_key1;
  uint8_t *local_ks_binpath = NULL;
  size_t local_ks_binpath_len;
  rw_keyspec_path_t *local_keyspec = NULL;
  /* Build the shard-key to get the shard-id and DB number */
  char str[15] = "banana";

  reg = mobj->reg;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  apih = reg->apih;
  RW_ASSERT(apih);
  msg = mobj->msg;

  shard_key1 = RW_MALLOC0_TYPE(sizeof(rwdts_shard_info_detail_t), rwdts_shard_info_detail_t);
  shard_key1->shard_key_detail.u.byte_key.k.key = (void *)RW_MALLOC(sizeof(str));
  memcpy((char *)shard_key1->shard_key_detail.u.byte_key.k.key, &str[0], strlen(str));
  shard_key1->shard_key_detail.u.byte_key.k.key_len = strlen(str);

  /* Get the shard-id and DB number */
  rwdts_shard_db_num_info_t *shard_db_num_info = RW_MALLOC0(sizeof(rwdts_shard_db_num_info_t));

  status = rw_keyspec_path_create_dup(reg->keyspec, &apih->ksi , &local_keyspec);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  status = rw_keyspec_path_get_binpath(local_keyspec, &apih->ksi ,
                                       (const uint8_t **)&local_ks_binpath,
                                       &local_ks_binpath_len);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  rwdts_member_get_shard_db_info_keyspec(apih, local_keyspec,
                                         shard_key1,
                                         shard_db_num_info);
  if (shard_db_num_info->shard_db_num_cnt > 0) {
    db_number = shard_db_num_info->shard_db_num[0].db_number;
    //shard_id = shard_db_num_info->shard_db_num[0].shard_chunk_id;
    /* Search for KV table handle */
    status = RW_SKLIST_LOOKUP_BY_KEY(&(apih->kv_table_handle_list), &db_number,
                                     (void *)&kv_tab_handle);
    if (!kv_tab_handle) {
      kv_tab_handle = rwdts_kv_light_register_table(apih->handle, db_number);
      RW_ASSERT(kv_tab_handle);
      status = RW_SKLIST_INSERT(&(apih->kv_table_handle_list), kv_tab_handle);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }
    mobj->kv_tab_handle = kv_tab_handle;
    RWDTS_CREATE_SHARD(reg->reg_id, apih->client_path, apih->router_path);
    /* Perform KV xact operation */
    if (apih->db_up && ((action == RWDTS_QUERY_CREATE) ||
        (RWDTS_QUERY_UPDATE == action))) {
      RW_ASSERT(msg);
      payload = protobuf_c_message_serialize(NULL, msg, &payload_len);
      rwdts_kv_light_table_insert(kv_tab_handle, 0, shard,
                                  (void *)mobj->key,
                                  mobj->key_len,
                                  payload,
                                  payload_len,
                                  (void *)rwdts_kv_light_insert_obj_cb,
                                  (void *)mobj);
    } else if (apih->db_up && (action == RWDTS_QUERY_DELETE)) {
      rwdts_kv_light_table_xact_delete(kv_tab_handle, 0,
                                       (void *)mobj->key,
                                       mobj->key_len,
                                       (void *)rwdts_kv_light_delete_obj_cb,
                                       (void *)mobj);
    }
  }
  if (shard_db_num_info->shard_db_num_cnt) {
    free(shard_db_num_info->shard_db_num);
  }
  RW_FREE(shard_db_num_info);
  rw_keyspec_path_free(local_keyspec, NULL);
  RW_FREE(shard_key1->shard_key_detail.u.byte_key.k.key);
  RW_FREE_TYPE(shard_key1, rwdts_shard_info_detail_t);
  return RW_STATUS_SUCCESS;
}

// This will be supported after sharding support
//
//LCOV_EXCL_START
rw_status_t
rwdts_kv_update_db_xact_precommit(rwdts_member_data_object_t *mobj, RWDtsQueryAction action)
{
  rwdts_api_t *apih;
  rwdts_member_registration_t *reg;
  ProtobufCMessage *msg;
  uint8_t *payload;
  size_t  payload_len;
  rw_status_t status;
  rwdts_kv_table_handle_t *kv_tab_handle = NULL;
  rwdts_shard_info_detail_t *shard_key1;
  uint8_t *local_ks_binpath = NULL;
  size_t local_ks_binpath_len;
  rw_keyspec_path_t *local_keyspec = NULL;
  /* Build the shard-key to get the shard-id and DB number */
  char str[15] = "banana";

  reg = mobj->reg;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  apih = reg->apih;
  RW_ASSERT(apih);
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  msg = mobj->msg;

  shard_key1 = RW_MALLOC0_TYPE(sizeof(rwdts_shard_info_detail_t), rwdts_shard_info_detail_t);
  shard_key1->shard_key_detail.u.byte_key.k.key = (void *)RW_MALLOC(sizeof(str));
  memcpy((char *)shard_key1->shard_key_detail.u.byte_key.k.key, &str[0], strlen(str));
  shard_key1->shard_key_detail.u.byte_key.k.key_len = strlen(str);

  /* Get the shard-id and DB number */
  rwdts_shard_db_num_info_t *shard_db_num_info = RW_MALLOC0(sizeof(rwdts_shard_db_num_info_t));

  status = rw_keyspec_path_create_dup(reg->keyspec, &apih->ksi , &local_keyspec);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  status = rw_keyspec_path_get_binpath(local_keyspec, &apih->ksi , (const uint8_t **)&local_ks_binpath, &local_ks_binpath_len);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  rwdts_member_get_shard_db_info_keyspec(apih, local_keyspec,
                                         shard_key1,
                                         shard_db_num_info);
  if (shard_db_num_info->shard_db_num_cnt > 0) {
    mobj->db_number = shard_db_num_info->shard_db_num[0].db_number;
    mobj->shard_id = shard_db_num_info->shard_db_num[0].shard_chunk_id;
    /* Search for KV table handle */
    status = RW_SKLIST_LOOKUP_BY_KEY(&(apih->kv_table_handle_list), &mobj->db_number,
                                     (void *)&kv_tab_handle);
    if (!kv_tab_handle) {
      kv_tab_handle = rwdts_kv_light_register_table(apih->handle, mobj->db_number);
      RW_ASSERT(kv_tab_handle);
      status = RW_SKLIST_INSERT(&(apih->kv_table_handle_list), kv_tab_handle);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }
    mobj->kv_tab_handle = kv_tab_handle;
    RWDTS_CREATE_SHARD(reg->reg_id, apih->client_path, apih->router_path);
    /* Perform KV xact operation */
    if (apih->db_up && ((action == RWDTS_QUERY_CREATE) ||
        (RWDTS_QUERY_UPDATE == action))) {
      RW_ASSERT(msg);
      payload = protobuf_c_message_serialize(NULL, msg, &payload_len);
      rwdts_kv_light_table_xact_insert(kv_tab_handle, 0, shard,
                                       (void *)mobj->key,
                                       mobj->key_len,
                                       payload,
                                       payload_len,
                                       (void *)rwdts_kv_light_insert_xact_obj_cb,
                                       (void *)mobj);
    } else if (apih->db_up && (action == RWDTS_QUERY_DELETE)) {
       rwdts_kv_light_table_xact_delete(kv_tab_handle, 0,
                                        (void *)mobj->key,
                                        mobj->key_len,
                                        (void *)rwdts_kv_light_delete_xact_obj_cb,
                                        (void *)mobj);
    }
  }
  if (shard_db_num_info->shard_db_num_cnt) {
    free(shard_db_num_info->shard_db_num);
  }
  RW_FREE(shard_db_num_info);
  rw_keyspec_path_free(local_keyspec, NULL);
  RW_FREE(shard_key1->shard_key_detail.u.byte_key.k.key);
  RW_FREE_TYPE(shard_key1, rwdts_shard_info_detail_t);
  return RW_STATUS_SUCCESS;
}

//LCOV_EXCL_STOP
