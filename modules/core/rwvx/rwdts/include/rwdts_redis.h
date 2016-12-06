/*
/   Copyright 2016 RIFT.IO Inc
/
/   Licensed under the Apache License, Version 2.0 (the "License");
/   you may not use this file except in compliance with the License.
/   You may obtain a copy of the License at
/
/       http://www.apache.org/licenses/LICENSE-2.0
/
/   Unless required by applicable law or agreed to in writing, software
/   distributed under the License is distributed on an "AS IS" BASIS,
/   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/   See the License for the specific language governing permissions and
/   limitations under the License.
/*/
/*!
 * @file rwdts_redis.h
 * @brief Top level API include for RW.DTS.REDIS
 * @author Vijay Nag(Vijay.Nag@riftio.com)
 * @date 2014/10/23
 */
#ifndef __RWDTS_REDIS_H
#define __RWDTS_REDIS_H

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
#include <hiredis/hiredis.h>

__BEGIN_DECLS

enum rwdts_reply_status {
  RWDTS_REDIS_REPLY_INVALID,
  RWDTS_REDIS_REPLY_DONE, /*done, delete the handle */
  RWDTS_REDIS_REPLY_KEEP, /*user stowed the handle */
  RWDTS_REDIS_REPLY_MAX = 0xff
};

typedef struct rwdts_redis_msg_handle rwdts_redis_msg_handle;
typedef struct rwdts_redis_instance rwdts_redis_instance_t;
typedef enum rwdts_reply_status rwdts_reply_status;
typedef struct rwdts_redis_db_init_info rwdts_redis_db_init_info;
typedef void (*rwdts_redis_callback_t)(rwdts_redis_db_init_info *db_info);
typedef rwdts_reply_status (*rwdts_trans_callback)(rwdts_redis_msg_handle*, void *userdata);

typedef enum rwdts_redis_state {
  RWDTS_REDIS_STATE_INVALID, /* The initial null state */
  RWDTS_REDIS_STATE_CONNECTING, /* State when connect(2) has been initiated */
  RWDTS_REDIS_STATE_AWAITING_CLUSTER_INFO, /* State after connecting awaiting cluster info from redis server */
  RWDTS_REDIS_STATE_CONNECTED, /* Connection is up and shard tables are available in the client */
  RWDTS_REDIS_CONNECTION_STATE_MAX = 0xff
} rwdts_redis_state;

typedef enum rwdts_redis_client_state {
  RWDTS_REDIS_CLIENT_STATE_INVALID,
  RWDTS_REDIS_CLIENT_STATE_CONNECTING,
  RWDTS_REDIS_CLIENT_STATE_CONNECTED,
  RWDTS_REDIS_CLIENT_STATE_MAX = 0xff
} rwdts_redis_client_state;

typedef struct rwdts_redis_stats {
  uint64_t redis_rx_commands;
  uint64_t redis_tx_commands;
} rwdts_redis_stats_t;

typedef struct rwdts_redis_node {
#define MAX_REDIS_HOST_NAME 128
  rwdts_redis_instance_t *instance;
  redisContext *ctxt;
  rwsched_dispatch_source_t source; /*libdispatch source for this client*/
  rwdts_redis_client_state state;
  char hostname[MAX_REDIS_HOST_NAME];
  uint8_t node_idx; /*index within instance->cluster.nodes*/
  uint16_t hash_start;
  uint16_t hash_last;
  uint16_t port;
  rwdts_redis_stats_t stats;
  rw_dl_t list;
} rwdts_redis_node_t;

#define MAX_REDIS_CLUSTER_NODES 8 /* 8 is the max as per Redis specification */
typedef struct rwdts_redis_cluster {
  rwdts_redis_node_t *nodes[MAX_REDIS_CLUSTER_NODES];
  uint8_t count;
} rwdts_redis_cluster_t;

#define RWDTS_REDIS_INSTANCE_MAGIC  0xA55E5
#define RWDTS_REDIS_VALID_INSTANCE(inst) ((inst)->magic == RWDTS_REDIS_INSTANCE_MAGIC)
struct rwdts_redis_instance {
  int                             magic;
  rwsched_instance_ptr_t          sched_instance;
  rwsched_tasklet_ptr_t      tasklet_info;
  rwsched_dispatch_queue_t        rwq;
  rwdts_redis_cluster_t           cluster;
  rwdts_redis_state               state;
  rwdts_redis_node_t              *primary_node;
  rwdts_redis_callback_t          callback_fn;
  void                            *userdata;
};

typedef enum rwdts_handle_status {
  RW_HANDLE_STATE_INVALID,
  RW_HANDLE_STATE_REPLY,
  RW_HANDLE_STATE_DELETE,
  RW_HANDLE_STATE_ERROR,
  RW_HANDLE_STATE_MAX,
} rwdts_handle_status_t;

#define RWDTS_HANDLE_MAGIC 0x00FF1CE /*no pun intended */
struct rwdts_redis_msg_handle {
  int h_magic;
  redisReply *reply;
  rwdts_handle_status_t status; /*user replied status */
  rwdts_trans_callback cb; /*user callback */
  void *ud; /*user data */
  rw_dl_element_t elem; /*list element it is part of */
};

typedef enum rwdts_redis_db_status {
  RWDTS_REDIS_DB_INVALID,
  RWDTS_REDIS_DB_UP,
  RWDTS_REDIS_DB_DOWN,
  RWDTS_REDIS_DB_MAX = 0xff
} rwdts_redis_db_status;

struct rwdts_redis_db_init_info {
  rwdts_redis_db_status status;
  rwdts_redis_instance_t *instance;
  const char *errstr;
  void *userdata;
};

#define RWDTS_REDIS_DB_STATUS(init_info) (init_info)->status
#define RWDTS_REDIS_DB_ERR_STR(init_info) (init_info)->errstr

extern rw_status_t  rwdts_redis_instance_init(rwsched_instance_ptr_t sched_instance,
                                              rwsched_tasklet_ptr_t  tasklet_info,
                                              char *hostname,
                                              uint16_t port,
                                              rwdts_redis_callback_t callback_fn,
                                              void *userdata);
extern void rwdts_print_cluster_info_for_key(rwdts_redis_instance_t *instance,
                                             char *key,
                                             int len);
extern void rwdts_redis_cmd(rwdts_redis_instance_t *instance,
                            int db_num, char *cmd,
                            char *key, int key_len, void *value,
                            int value_len, rwdts_trans_callback callback,
                            void *userdata);

extern void rwdts_redis_hash_cmd(rwdts_redis_instance_t *instance,
                                 int db_num, char *cmd, char *key,
                                 int serial_num, void *value,
                                 int value_len, rwdts_trans_callback callback,
                                 void *userdata);

extern int rwdts_redis_load_script(rwdts_redis_instance_t *instance,
                                   const char *filename,
                                   rwdts_trans_callback callback,
                                   void *userdata);
/*
extern void rwdts_redis_run_script(rwdts_redis_instance_t *instance,
                                   const char *sha_script,
                                   char *key, int key_len,
                                   int index, int count,
                                   rwdts_trans_callback callback,
                                   void *userdata);
                                   */

extern int rwdts_redis_run_get_next_script(rwdts_redis_instance_t *instance,
                                           const char *sha_script, int db_num,
                                           uint32_t *scan_list, char *shard_num,
                                           rwdts_trans_callback callback,
                                           void *userdata);

extern rw_status_t rwdts_redis_run_set_script(rwdts_redis_instance_t *instance,
                                              const char *sha_script, char *key,
                                              int key_len, int db_num, int serial_num,
                                              char *shard_num,  char *value, int val_len,
                                              rwdts_trans_callback callback,
                                              void *userdata);

extern rw_status_t 
rwdts_redis_run_shash_set_script(rwdts_redis_instance_t *instance,
                                 const char *sha_script, char *key,
                                 int key_len, int db_num, int serial_num,
                                 char *shard_num,  char *value, int val_len,
                                 rwdts_trans_callback callback,
                                 void *userdata);

extern rw_status_t
rwdts_redis_run_single_command(rwdts_redis_instance_t *instance,
                               char *command, const char *sha_script, 
                               char *key, int key_len, int db_num, char *value,
                               int val_len, int serial_num, 
                               rwdts_trans_callback callback,
                               void *userdata);

extern void rwdts_redis_run_del_script(rwdts_redis_instance_t *instance,
                                       const char *sha_script, char *key,
                                       int key_len, int db_num, int serial_num,
                                       rwdts_trans_callback callback,
                                       void *userdata);

extern int rwdts_redis_run_get_script(rwdts_redis_instance_t *instance,
                                      const char *sha_script, int db_num,
                                      rwdts_trans_callback callback,
                                      void *userdata);

extern int rwdts_redis_run_del_shard_script(rwdts_redis_instance_t *instance,
                                            const char *sha_script, int db_num,
                                            char *shard_num, rwdts_trans_callback callback,
                                            void *userdata);

extern void rwdts_redis_run_set_pend_commit_abort_script(rwdts_redis_instance_t *instance,
                                                         const char *sha_script, char *key,
                                                         int key_len, int db_num, int serial_num,
                                                         char *shard_num, rwdts_trans_callback callback,
                                                         void *userdata);

extern int rwdts_redis_del_table(rwdts_redis_instance_t *instance, int db_num,
                                 rwdts_trans_callback callback, void *userdata);

extern int rwdts_redis_run_get_all_shash_script(rwdts_redis_instance_t *instance,
                                                const char *sha_script, int db_num,
                                                char *shard_num, rwdts_trans_callback callback,
                                                void *userdata);

extern rw_status_t rwdts_redis_run_del_shash_script(rwdts_redis_instance_t *instance,
                                                    const char *sha_script, int db_num,
                                                    char *shard_num, char *key, int key_len,
                                                    rwdts_trans_callback callback, void *userdata);
extern void rwdts_redis_slave_to_master(rwdts_redis_instance_t *instance,
                                        rwdts_trans_callback callback,
                                        void *userdata);

extern void rwdts_redis_master_to_slave(rwdts_redis_instance_t *instance,
                                        char *hostname, uint16_t port,
                                        rwdts_trans_callback callback,
                                        void *userdata);

#define rwdts_redis_set_key_value(inst, db_num, key, key_len, val, val_len, cb,\
                                  ud)\
  rwdts_redis_cmd(inst, db_num, (char *)"SET", key, key_len, val, val_len,\
                  cb, ud);

#define rwdts_redis_get_key_value(handle, db_num, key, key_len, cb, ud)\
  rwdts_redis_run_single_command((rwdts_redis_instance_t *)handle->kv_conn_instance, \
                                 (char *)"GET", handle->single_command, key, key_len, db_num, \
                                 NULL, 0, 0, cb, ud);

#define rwdts_redis_del_key_value(inst, db_num, key, key_len, cb, ud)\
  rwdts_redis_cmd(inst, db_num, (char *)"DEL", key, key_len, NULL, 0, cb, ud);

#define rwdts_redis_get_hash_key_value(handle, db_num, key, key_len, cb, ud)\
  rwdts_redis_run_single_command((rwdts_redis_instance_t *)handle->kv_conn_instance, \
                                 (char *)"HVALS", handle->single_command, key, key_len, db_num, \
                                 NULL, 0, 0, cb, ud);

#define rwdts_redis_get_hash_key_sernum_value(instance, db_num, key, key_len, cb, ud)\
    rwdts_redis_run_single_command((rwdts_redis_instance_t *)handle->kv_conn_instance, \
                                   (char *)"HGETALL", handle->single_command, key, key_len, db_num, \
                                   NULL, 0, 0, cb, ud); 

#define rwdts_redis_del_hash_key_value(inst, db_num, serial_num, cb,\
                                       ud, hash)\
  rwdts_redis_hash_cmd(inst, db_num, (char *)"HDEL", hash, serial_num, NULL, 0, \
                       cb, ud);

#define rwdts_redis_get_all_hash(inst, db_num, cb, ud, hash)\
  rwdts_redis_hash_cmd(inst, db_num, (char *)"HGETALL", hash, 0, NULL, 0, \
                       cb, ud);

#define rwdts_redis_get_hash_len(inst, db_num, cb, ud, hash)\
    rwdts_redis_hash_cmd(inst, db_num, (char *)"HLEN", hash, 0, NULL, 0,\
                         cb, ud);

#define rwdts_redis_update_hash_key_value(inst, db_num, serial_num, val,\
                                          val_len, cb, ud, hash)\
  rwdts_redis_hash_cmd(inst, db_num, (char *)"HSETNX", hash, serial_num, val, val_len,\
                       cb, ud);

#define rwdts_redis_hash_exist_key_value(inst, db_num, key, key_len, cb, ud)\
  rwdts_redis_run_single_command((rwdts_redis_instance_t *)handle->kv_conn_instance, \
                                 (char *)"EXISTS", handle->single_command, key, key_len, db_num, \
                                 NULL, 0, 0, cb, ud); 

extern void rwdts_redis_instance_deinit(rwdts_redis_instance_t *instance);

#define REDIS_HANDLE_ALLOC()\
({ \
   void *ptr = NULL;                                                                  \
   ptr = RW_MALLOC0_TYPE(sizeof(rwdts_redis_msg_handle), rwdts_redis_msg_handle);     \
   RW_ASSERT(ptr);                                                                    \
   ptr;                                                                               \
});

#define REDIS_HANDLE_DEALLOC(ptr)\
({\
    RW_FREE_TYPE(ptr, rwdts_redis_msg_handle); \
})

#define RW_REDIS_ENQUEUE(node, handle) RW_DL_ENQUEUE(&node->list, handle, elem);
#define RW_REDIS_DEQUEUE(node) RW_DL_DEQUEUE(&node->list, rwdts_redis_msg_handle, elem)

#define RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, udata) \
({\
   RW_ASSERT(callback);                               \
   rwdts_redis_msg_handle *h = REDIS_HANDLE_ALLOC();  \
   h->h_magic = RWDTS_HANDLE_MAGIC;                   \
   h->cb = callback;                                  \
   h->ud = udata;                                     \
   h->reply = NULL;                                   \
   h->status = RWDTS_REDIS_REPLY_INVALID;             \
   RW_REDIS_ENQUEUE(node, h);                         \
   h;                                                 \
})

#define RWDTS_DESTROY_REDIS_REPLY(reply) \
({                                       \
   RW_ASSERT(reply);                     \
   freeReplyObject(h->reply);            \
})

#define RWDTS_DESTROY_HANDLE(h) \
({\
    RW_ASSERT(RWDTS_HANDLE_MAGIC == h->h_magic);   \
    h->h_magic = 0xdeadbeef;                       \
    if (h->reply) {                                \
      freeReplyObject(h->reply);                   \
      h->reply = NULL;                             \
    }                                              \
    REDIS_HANDLE_DEALLOC(h);                       \
})

#define RWDTS_FLUSH_REDIS_HANDLE(node) \
({\
 rwdts_redis_msg_handle *h = NULL;                    \
 uint32_t length = rw_dl_walking_length(&node->list); \
 while(length) {                                      \
   h = RW_REDIS_DEQUEUE(node);                        \
   RW_ASSERT(h);                                      \
   h->status = RW_HANDLE_STATE_ERROR;                 \
   h->cb(h, h->ud);                                   \
   RWDTS_DESTROY_HANDLE(h);                           \
   length--;                                          \
 }                                                    \
})

#define RWDTS_VIEW_REPLY(h)  (void*)((RW_HANDLE_STATE_REPLY == h->status) ? h->reply->str : NULL)
#define RWDTS_VIEW_REPLY_LEN(h)  (RW_HANDLE_STATE_REPLY == h->status) ? h->reply->len : 0
#define RWDTS_VIEW_REPLY_STATUS(h) h->status
#define RWDTS_VIEW_REPLY_INT(h) ((RW_HANDLE_STATE_REPLY == h->status) ? h->reply->integer : 0)
#define RWDTS_DEL_SUCCESSFUL(h) (0 == RWDTS_VIEW_REPLY_INT(h)) ? 0 : 1

__END_DECLS

#endif /*__RWDTS_REDIS_H */

