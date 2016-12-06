
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


/*!
 * @file rwdts_shard.h
 * @brief RW.DTS sharding
 * @date Aug 19 2015
 */

#ifndef __RWDTS_SHARD_H
#define __RWDTS_SHARD_H

#include <libgen.h>
#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwmsg.h>
#include <rw_dts_int.pb-c.h>
#include <rw_sklist.h>
#include <rwdts_query_api.h>
#include <rwdts_member_api.h>
#include <uthash.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include "rw-dts-api-log.pb-c.h"
#include "rw-dts-router-log.pb-c.h"
#include "rw-dts.pb-c.h"
#include <rwlog.h>
//#include <rwdts_router.h>


// Hash defines
#define RWDTS_SHARD_CHUNK_SZ 100 // 100 records per chunk

typedef struct rwdts_router_member_s rwdts_router_member_t;

typedef struct rwdts_shard_info_detail_s {
  rwdts_shard_key_detail_t shard_key_detail;
  rwdts_shard_flavor shard_flavor;
  union rwdts_shard_flavor_params_u params; 
  bool has_index;
  int index;
} rwdts_shard_info_detail_t;

typedef  struct rwdts_shard_chunk_router_s {
// put router specific data to be stored in chunk
// members who registered for this keyspec etc
  UT_hash_handle        hh_rtr_record;
  rwdts_router_member_t *member;
  char msgpath[256];  /* TBD what is max msg path size? */
  uint32_t flags;
} rwdts_chunk_rtr_info_t;

typedef struct rwdts_shard_chunk_member_s {
  UT_hash_handle        hh_mbr_record;
  rwdts_member_reg_handle_t member;
  char msgpath[256];  /* TBD what is max msg path size? */
  uint32_t flags;
} rwdts_chunk_member_info_t;

typedef struct rwdts_chunk_full_rtr_info_s {
  int32_t n_pubrecords;
  int32_t n_subrecords;
  rwdts_chunk_rtr_info_t    *pub_rtr_info;
  rwdts_chunk_rtr_info_t    *sub_rtr_info;
} rwdts_chunk_full_rtr_info_t;

typedef struct rwdts_chunk_full_mem_info_s {
  int                       n_sub_reg;
  int                       n_pub_reg;
  rwdts_chunk_member_info_t *  pub_mbr_info;
  rwdts_chunk_member_info_t *  sub_mbr_info;
} rwdts_chunk_full_mem_info_t;

union rwdts_shard_chunk_record_u {
  rwdts_chunk_full_rtr_info_t rtr_info;
  rwdts_chunk_full_mem_info_t member_info;
};
     
typedef struct rwdts_chunk_key_null_s {
} rwdts_chunk_key_null_t;

typedef struct rwdts_chunk_key_ident_s {
  rw_schema_minikey_opaque_t key;
  size_t  keylen;
} rwdts_chunk_key_ident_t;

union rwdts_shard_chunk_key_u {
  rwdts_chunk_key_null_t nullkey;
  rwdts_chunk_key_ident_t identkey;
  struct rwdts_shard_range_params_s rangekey;
};

struct rwdts_shard_chunk_info_s {
  UT_hash_handle   hh_chunk;
  rwdts_chunk_id_t chunk_id;
  int              ref_cnt;
  union rwdts_shard_chunk_record_u elems;
  union rwdts_shard_chunk_key_u chunk_key;
  struct rwdts_shard_s *parent;
};

typedef struct rwdts_shard_chunk_info_s rwdts_shard_chunk_info_t;

typedef enum {
  RWDTS_SHARD_ELMID=1,
  RWDTS_SHARD_KEY  =2
} rwdts_shard_key_type;

typedef struct rwdts_shard_appdata_object_s {
  UT_hash_handle hh_data;
  uint8_t *key;
  uint32_t key_len;
  ProtobufCMessage *msg;
  rw_keyspec_path_t *keyspec;
  rw_keyspec_entry_t *pathentry;
  rw_schema_minikey_t *minikey;
} rwdts_shard_appdata_object_t;

struct rwdts_shard_s {
  UT_hash_handle                     hh_shard;
  uint8_t *                          key;
  size_t                             key_len;
  rwdts_shard_flavor                 flavor;
  struct rwdts_shard_s *             parent;
  struct rwdts_shard_s *             children;
  struct rwdts_shard_s *             wildcard_shard;
  union rwdts_shard_flavor_params_u  flavor_params;
  enum rwdts_shard_keyfunc_e         hash_func;
  union rwdts_shard_keyfunc_params_u keyfunc_params;
  enum rwdts_shard_anycast_policy_e  anycast_policy;
  rwdts_member_getnext_cb *          prepare;
  uint64_t                           shard_id;
  int                                ref_cnt;
  rwdts_shard_chunk_info_t *         shard_chunks;
  uint64_t                           client_idx;
  rwdts_chunk_id_t                   next_chunk_id;
//uint32_t                            flags;
//rwdts_member_reg_handle_t *        sub_reg;
//rwdts_member_reg_handle_t *        pub_reg;
//int                                n_sub_reg;
//int                                n_pub_reg;
  uint32_t                           key_index;
  uint32_t                           pe_index;
  rwdts_shard_key_type               key_type;                         
  rwdts_appdata_t *                  appdata;
  rwdts_api_t *                      apih;
  rwdts_shard_appdata_object_t *     obj_list;
  rw_sklist_element_t                element;
};


struct rwdts_shard_chunk_iter_s {
  rwdts_shard_t       *shard;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_chunk_id_t    chunk_id;
};
typedef struct rwdts_shard_chunk_iter_s rwdts_shard_chunk_iter_t;

struct rwdts_shard_iter_s {
  rwdts_shard_t       *shard;
  rwdts_shard_chunk_iter_t *chunk_iter;
  rw_keyspec_path_t *keyspec;
  union rwdts_shard_flavor_params_u params;
};

typedef struct rwdts_shard_iter_s rwdts_shard_iter_t;

struct rwdts_shard_nsid_tag {
  uint32_t nsid;
  uint32_t tag;
};
union rwdts_shard_elemkey_u  {
  uint8_t key[8];
};

typedef union rwdts_shard_elemkey_u rwdts_shard_elemkey_t;

__BEGIN_DECLS

void rwdts_shard_handle_unref(rwdts_shard_t *s, const char *file, int line);
rwdts_shard_t *rwdts_shard_handle_ref(rwdts_shard_t *s, const char *file, int line);
rwdts_shard_handle_t *
rwdts_shard_init_keyspec(rw_keyspec_path_t *, int , rwdts_shard_t **,
                             rwdts_shard_flavor,
                             union rwdts_shard_flavor_params_u *,
                             enum rwdts_shard_keyfunc_e ,
                             union rwdts_shard_keyfunc_params_u *,
                             enum rwdts_shard_anycast_policy_e );
rwdts_shard_t *
rwdts_shard_match_keyspec(rwdts_shard_handle_t *rootshard,
                          rw_keyspec_path_t *keyspec,
                          union rwdts_shard_flavor_params_u *,
                          rwdts_shard_chunk_info_t **chunk_out,
                          rwdts_chunk_id_t *chunk_id_out);
rw_status_t
rwdts_shard_deinit_keyspec(rw_keyspec_path_t *keyspec,
                             rwdts_shard_t **parent);
rwdts_shard_chunk_info_t *
rwdts_shard_add_chunk(rwdts_shard_handle_t *shard, union rwdts_shard_flavor_params_u *params); 

rw_status_t
rwdts_shard_delete_chunk(rwdts_shard_handle_t *shard, union rwdts_shard_flavor_params_u *params);

rw_status_t
rwdts_rts_shard_create_element(rwdts_shard_t *shard, rwdts_shard_chunk_info_t *chunk, 
                               rwdts_chunk_rtr_info_t *rtr_info, bool publisher,
                               rwdts_chunk_id_t *chunk_id, uint32_t *membid, char *msgpath);

rw_status_t
rwdts_rts_shard_update_element(rwdts_shard_t *shard, rwdts_chunk_rtr_info_t *rtr_info,
                   bool publisher, rwdts_chunk_id_t chunk_id, uint32_t membid, char *msgpath);

bool
rwdts_rtr_shard_chunk_iter_next(rwdts_shard_chunk_iter_t *iter,
      rwdts_chunk_rtr_info_t **records_out, int32_t *n_records_out);

rwdts_shard_chunk_iter_t *
rwdts_shard_chunk_iter_create_keyspec(rwdts_shard_t *rootshard,
      rw_keyspec_path_t *keyspec, union rwdts_shard_flavor_params_u *params);

bool
rwdts_shard_match_pathelem(rwdts_shard_t **head, const rw_keyspec_entry_t *pe, rwdts_shard_t ***shards, uint32_t *n_shards);

void
rwdts_shard_match_pathelm_recur(rw_keyspec_path_t        *keyspec,
                                rwdts_shard_t            *root,
                                rwdts_shard_chunk_info_t ***shards,
                                uint32_t                 *n_chunks,
                                union rwdts_shard_flavor_params_u *params);


rw_status_t rwdts_shard_deref(rwdts_shard_t *shard);

rw_status_t
rwdts_member_api_shard_key_init(const rw_keyspec_path_t*        keyspec,
                              rwdts_api_t*                      apih,
                              const ProtobufCMessageDescriptor* desc,
                              uint32_t                          flags,
                              int                               idx,
                              rwdts_shard_flavor              flavor,
                              union rwdts_shard_flavor_params_u *flavor_params,
                              enum rwdts_shard_keyfunc_e         hashfunc,
                              union rwdts_shard_keyfunc_params_u *keyfunc_params,
                              enum rwdts_shard_anycast_policy_e   anypolicy,
                              rwdts_member_event_cb_t             *cb);

bool
rwdts_shard_range_key_compare(const rw_keyspec_entry_t *pe,
                              rw_schema_minikey_opaque_t *mk,
                              union rwdts_shard_chunk_key_u *params);

rw_status_t
rwdts_shard_update(rwdts_shard_t *shard,
                   rwdts_shard_flavor flavor,
                   union rwdts_shard_flavor_params_u *flavor_params);
rw_status_t
rwdts_member_api_shard_key_update(const rw_keyspec_path_t*      keyspec,
                              rwdts_api_t*                      apih,
                              rwdts_member_reg_handle_t         regh,
                              const ProtobufCMessageDescriptor* desc,
                              int                               idx,
                              rwdts_shard_flavor                flavor,
                              union rwdts_shard_flavor_params_u *flavor_params,
                              enum rwdts_shard_keyfunc_e         hashfunc,
                              union rwdts_shard_keyfunc_params_u *keyfunc_params,
                              enum rwdts_shard_anycast_policy_e   anypolicy,
                              rwdts_member_event_cb_t             *cb);
rwdts_shard_t *
rwdts_shard_chunk_match(rwdts_shard_handle_t *shard_handle,
                  rw_keyspec_path_t *ks,
                  union rwdts_shard_flavor_params_u *params,
                  rwdts_shard_chunk_info_t **chunk_out,
                  rwdts_chunk_id_t *chunk_id_out);

rw_status_t
rwdts_shard_rtr_delete_element(rwdts_shard_t *shard, char *member, bool pub);
rw_status_t
rwdts_shard_member_delete_element(rwdts_shard_t *shard, char *member, bool pub);

rw_status_t
rwdts_member_shard_create_element(rwdts_shard_t *shard, rwdts_shard_chunk_info_t *chunk,
                     rwdts_chunk_member_info_t *mbr_info, bool publisher,
                     rwdts_chunk_id_t *chunk_id, char *msgpath);

rw_status_t
rwdts_rts_shard_promote_element(rwdts_shard_t *shard, rwdts_chunk_id_t chunk_id, 
                                uint32_t membid, char *msgpath);

rw_status_t
rwdts_member_shard_promote_to_publisher(rwdts_shard_handle_t *shard, char* member);
__END_DECLS

#endif  /*__RWDTS_SHARD_H */
