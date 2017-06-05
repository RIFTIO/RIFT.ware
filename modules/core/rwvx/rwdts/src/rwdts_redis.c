/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwdts_redis.c
 * @author Vijay Nag (Vijay.Nag@riftio.com)
 * @date 2014/09/23
 * @brief Redis Client CORE API Stuff
 */

#include <rwdts_redis.h>
#include <hiredis/hiredis.h>
#include <hiredis/sds.h>

#define RWDTS_REDIS_INCREMENT_STATS(redis_node, x)     \
({                                                     \
    redis_node->stats.redis_ ## x ## _commands++;          \
})

#define MAX_REDIS_HASH_SLOTS 16384

static void rwdts_build_client_table(rwdts_redis_instance_t *instance);
static uint8_t rwdts_hash_slot_to_node_idx(rwdts_redis_instance_t *instance,
                                           uint16_t hashslot);
static unsigned int rwdts_key_hash_slot(char *key, int keylen);

struct cluster_info {
  struct {
    int hash_start;
    int hash_last;
    char hostname[MAX_REDIS_HOST_NAME];
    int port;
  } slot_info[MAX_REDIS_CLUSTER_NODES];
  int count;
  int not_cluster;
} cinfo;

/*
 * redis cluster info format
 * [cluster[1]: [<hash_start<INT>, hash_end<INT>, ip-addr<string>, port<INT>]],
 *  cluster[2]: [<hash_start<INT>, hash_end<INT>, ip-addr<string>, port<INT>]],
 * ..........
 *  cluster[N]: [<hash_start<INT>, hash_end<INT>, [ip-addr<string>, port<INT>]]
 * ]
 *
 * [] - represents REDIS_TYPE_ARRAY
 * The below routine recursively builds a cluster shard table
 */

static void ParseClusterSlotsCmd(redisReply *r)
{
    int i = 0;
    static int array_depth = 0; /*level of recursion */
    static enum {
      PARSE_STATE_READ_HASH_START,
      PARSE_STATE_READ_HASH_LAST,
      PARSE_STATE_READ_HOSTNAME,
      PARSE_STATE_READ_PORT
    } parse_state = PARSE_STATE_READ_HASH_START;

    RW_ASSERT(r);
    switch (r->type) {
    case REDIS_REPLY_NIL:
    case REDIS_REPLY_ERROR:
        fprintf(stdout,"Reply error: %s\n", r->str);
        cinfo.not_cluster = 1; /*may be not in cluster node*/
        break;
    case REDIS_REPLY_STRING:
        if (PARSE_STATE_READ_HOSTNAME == parse_state) {
          strncpy(cinfo.slot_info[cinfo.count].hostname,r->str,r->len);
          parse_state = PARSE_STATE_READ_PORT;
        }
        break;
    case REDIS_REPLY_INTEGER:
        if (PARSE_STATE_READ_HASH_START == parse_state) {
          cinfo.slot_info[cinfo.count].hash_start = r->integer;
          parse_state = PARSE_STATE_READ_HASH_LAST;
        } else if (PARSE_STATE_READ_HASH_LAST == parse_state) {
          cinfo.slot_info[cinfo.count].hash_last = r->integer;
          parse_state = PARSE_STATE_READ_HOSTNAME;
        } else if (PARSE_STATE_READ_PORT == parse_state) {
          cinfo.slot_info[cinfo.count].port = r->integer;
          parse_state = PARSE_STATE_READ_HASH_START;
        }
        break;
    case REDIS_REPLY_ARRAY:
        array_depth++;
        for (i = 0; i < r->elements; i++) {
            ParseClusterSlotsCmd(r->element[i]);
            if (1 == array_depth) {
              cinfo.count++;
            }
        }
        array_depth--;
        break;
    default:
        fprintf(stdout,"Unknown reply type: %d\n", r->type);
        exit(1);
    }
}

static void PrintClusterInfo(void)
{
  int i = 0;

  for(i = 0; i < cinfo.count; ++i) {
    fprintf(stdout, "cluster[%d]: [%d, %d, %s, %d]\n",
        i, cinfo.slot_info[i].hash_start,
        cinfo.slot_info[i].hash_last,
        cinfo.slot_info[i].hostname,
        cinfo.slot_info[i].port);
  }
}

static void ReadCallback(void *userdata)
{
  rwdts_redis_node_t *redis_node = (rwdts_redis_node_t *)userdata;
  rwdts_redis_instance_t *instance = redis_node->instance;
  RW_ASSERT(redis_node);
  RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
  redisReply *reply = NULL;
  int retval = REDIS_OK;

  redisContext *ctxt = redis_node->ctxt;
  if (RWDTS_REDIS_STATE_AWAITING_CLUSTER_INFO == instance->state) {
    /* Note: It is important to read this cluster slot command
     * reply. It is ok to block here till we get a reply.
     */
    do {
      if (redisBufferRead(ctxt) == REDIS_ERR)
         retval = REDIS_ERR;
      if (redisGetReplyFromReader(ctxt, (void **)&reply) == REDIS_ERR)
         retval = REDIS_ERR;
    } while (reply == NULL);
    rwdts_redis_db_init_info init_data;
    if (REDIS_OK != retval) {
      init_data.status = RWDTS_REDIS_DB_DOWN;
      init_data.instance = NULL;
      init_data.errstr = ctxt->errstr;
      init_data.userdata = instance->userdata;
      instance->callback_fn(&init_data);
      rwdts_redis_instance_deinit(instance);
    } else {
      ParseClusterSlotsCmd(reply);
      PrintClusterInfo();
      rwdts_build_client_table(instance);
      instance->state = RWDTS_REDIS_STATE_CONNECTED;
      RWDTS_REDIS_INCREMENT_STATS(redis_node, rx);
      init_data.status = RWDTS_REDIS_DB_UP;
      init_data.instance = instance;
      init_data.userdata = instance->userdata;
      instance->callback_fn(&init_data);
      freeReplyObject(reply); /*for now free here*/
    }
  } else if (RWDTS_REDIS_STATE_CONNECTED == instance->state) {
    rwdts_redis_msg_handle *h = NULL;
    rwdts_reply_status status = RWDTS_REDIS_REPLY_INVALID;
    /*client transactions*/
    do {
      if ((redisBufferRead(ctxt) == REDIS_ERR))
        return;
      if ((REDIS_ERR == redisGetReplyFromReader(ctxt, (void **)&reply)))
        return;
      if (NULL == reply)
        return;
      RWDTS_REDIS_INCREMENT_STATS(redis_node, rx);
      h = RW_REDIS_DEQUEUE(redis_node);
      if (REDIS_REPLY_STATUS == reply->type ||
          REDIS_REPLY_STRING == reply->type ||
          REDIS_REPLY_INTEGER == reply->type ||
          REDIS_REPLY_ARRAY == reply->type) {
        h->status = RW_HANDLE_STATE_REPLY;
        h->reply = reply;
      } else {
        h->status = RW_HANDLE_STATE_ERROR;
        h->reply = NULL;
      }
      status =  h->cb(h, h->ud); /*we can do dispatch async in future */
      if (RWDTS_REDIS_REPLY_KEEP == status) {
        h->reply = NULL; /*user stowed the handle */
      }
      RWDTS_DESTROY_HANDLE(h);
    } while( NULL != reply);
  }
}

static void ConnectionUpCallback(void *userdata)
{
  rwdts_redis_node_t *redis_node = (rwdts_redis_node_t *)userdata;
  RW_ASSERT(redis_node);
  int len = 0;
  int status;

  rwdts_redis_instance_t *instance = redis_node->instance;
  RW_ASSERT(instance);
  RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
  redisContext *ctxt = redis_node->ctxt;
  RW_ASSERT(ctxt);

  if (RWDTS_REDIS_STATE_CONNECTING == instance->state) {
    /*
     * send the initial cluster slots command
     */
    redisAppendCommand(redis_node->ctxt, "cluster slots");
  }

  do {
    if (REDIS_ERR == (status = redisBufferWrite(ctxt, &len))) {
      break;
    }
    RWDTS_REDIS_INCREMENT_STATS(redis_node, tx);
  } while(!len);

  if (REDIS_OK == status) {
   if (RWDTS_REDIS_STATE_CONNECTING == instance->state)
     instance->state = RWDTS_REDIS_STATE_AWAITING_CLUSTER_INFO;
  } else {
    if (RWDTS_REDIS_STATE_CONNECTING == instance->state) {
      rwdts_redis_db_init_info init_data;
      init_data.status = RWDTS_REDIS_DB_DOWN;
      init_data.instance = NULL;
      init_data.errstr = ctxt->errstr;
      init_data.userdata = instance->userdata;
      instance->callback_fn(&init_data);
      instance->cluster.nodes[0] = redis_node;
      instance->cluster.count++;
      rwdts_redis_instance_deinit(instance);
      return;
    } else {
      /* TODO: Peer is dead or non existent ?
       * not sure whether to re-connect or
       * issue bounce to upper layer ?
       */
    }
  }
  redis_node->state = RWDTS_REDIS_CLIENT_STATE_CONNECTED;
  rwsched_dispatch_source_cancel(instance->tasklet_info,
                                 redis_node->source);

  redis_node->source = rwsched_dispatch_source_create(instance->tasklet_info,
                                                      RWSCHED_DISPATCH_SOURCE_TYPE_READ,
                                                      redis_node->ctxt->fd,
                                                      0,
                                                      instance->rwq);

   rwsched_dispatch_set_context(instance->tasklet_info,
                                redis_node->source, (void *)redis_node);

   rwsched_dispatch_source_set_event_handler_f(instance->tasklet_info,
                                               redis_node->source,
                                               ReadCallback);

   rwsched_dispatch_resume(instance->tasklet_info,
                           redis_node->source);

}

static rwdts_redis_node_t *rwdts_create_cluster_node(rwdts_redis_instance_t *instance,
                                                     char *hostname,
                                                     uint16_t port)
{
  RW_ASSERT(instance);
  RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));

  rwdts_redis_node_t *redis_node = RW_MALLOC0_TYPE(sizeof(rwdts_redis_node_t), rwdts_redis_node_t);
  RW_ASSERT(redis_node);
  redis_node->port = port;
  redis_node->state = RWDTS_REDIS_CLIENT_STATE_INVALID;
  redis_node->instance = instance;
  strncpy(redis_node->hostname, hostname, sizeof(redis_node->hostname));
  return redis_node;
}

static rw_status_t rwdts_connect_cluster_node(rwdts_redis_node_t *redis_node)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rwdts_redis_instance_t *instance = redis_node->instance;
  RW_ASSERT(instance);
  RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));

  redis_node->ctxt = redisConnectNonBlock(redis_node->hostname, redis_node->port);
  RW_ASSERT(redis_node->ctxt);
  redis_node->state = RWDTS_REDIS_CLIENT_STATE_CONNECTING;

  /* let clients connect in async fashion */
  redis_node->source = rwsched_dispatch_source_create(instance->tasklet_info,
                                                      RWSCHED_DISPATCH_SOURCE_TYPE_WRITE,
                                                      redis_node->ctxt->fd,
                                                      0,
                                                      instance->rwq);

   rwsched_dispatch_set_context(instance->tasklet_info,
                                redis_node->source, (void *)redis_node);

   rwsched_dispatch_source_set_event_handler_f(instance->tasklet_info,
                                               redis_node->source,
                                               ConnectionUpCallback);

   rwsched_dispatch_resume(instance->tasklet_info,
                           redis_node->source);


  return status;
}

static rw_status_t  rwdts_create_primary_cluster_node(rwdts_redis_instance_t *instance,
                                                      char *hostname,
                                                      uint16_t port)
{
   rwdts_redis_node_t *redis_node;
   RW_ASSERT(instance);
   RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));

   redis_node = rwdts_create_cluster_node(instance, hostname, port);
   if(NULL == redis_node) {
     return RW_STATUS_FAILURE;
   }
   if (RW_STATUS_SUCCESS != rwdts_connect_cluster_node(redis_node)) {
     return RW_STATUS_FAILURE;
   }
   instance->primary_node = redis_node;
   return RW_STATUS_SUCCESS;
}

rw_status_t rwdts_redis_instance_init(rwsched_instance_ptr_t        sched_instance,
                                      rwsched_tasklet_ptr_t    tasklet_info,
                                      char *hostname,
                                      uint16_t port,
                                      rwdts_redis_callback_t callback_fn,
                                      void *userdata)
{
  RW_ASSERT(sched_instance);
  RW_ASSERT(tasklet_info);

  rwdts_redis_instance_t *redis_instance = RW_MALLOC0_TYPE(sizeof(rwdts_redis_instance_t), rwdts_redis_instance_t);
  RW_ASSERT(redis_instance);
  if (NULL == redis_instance) return RW_STATUS_FAILURE;

  redis_instance->magic = RWDTS_REDIS_INSTANCE_MAGIC;
  redis_instance->sched_instance = sched_instance;
  redis_instance->tasklet_info = tasklet_info;
  redis_instance->callback_fn = callback_fn;
  redis_instance->userdata = userdata;
  cinfo.count = 0;
  /* lets be in main Queue for now */
  redis_instance->rwq = rwsched_dispatch_get_main_queue(sched_instance);
  RW_ASSERT(redis_instance->rwq);

  redis_instance->state = RWDTS_REDIS_STATE_CONNECTING;
  return rwdts_create_primary_cluster_node(redis_instance, hostname, port);
}

static void rwdts_build_client_table(rwdts_redis_instance_t *instance)
{
  int i = 0;
  rwdts_redis_node_t *node = NULL;

  RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
  if (cinfo.not_cluster) {
    instance->cluster.nodes[instance->cluster.count] = instance->primary_node;
    instance->cluster.nodes[instance->cluster.count]->hash_start = 0;
    instance->cluster.nodes[instance->cluster.count]->hash_last = MAX_REDIS_HASH_SLOTS;
    instance->cluster.nodes[instance->cluster.count++]->node_idx = 0;
    return;
  }

  for ( i = 0; i < cinfo.count; ++i) {
    if ((cinfo.slot_info[i].port == instance->primary_node->port) && \
        (!strcmp(cinfo.slot_info[i].hostname, instance->primary_node->hostname))) {
      node = instance->primary_node;
    } else {
      node = rwdts_create_cluster_node(instance,
                                       cinfo.slot_info[i].hostname,
                                       cinfo.slot_info[i].port);
    }
    node->hash_start = cinfo.slot_info[i].hash_start;
    node->hash_last = cinfo.slot_info[i].hash_last;
    node->node_idx = instance->cluster.count;
    instance->cluster.nodes[instance->cluster.count++] = node;
  }
}

/* We have 16384 hash slots. The hash slot of a given key is obtained
 * as the least significant 14 bits of the crc16 of the key.
 *
 * However if the key contains the {...} pattern, only the part between
 * { and } is hashed. This may be useful in the future to force certain
 * keys to be in the same node (assuming no resharding is in progress). */
static unsigned int rwdts_key_hash_slot(char *key, int keylen)
{
    int s, e; /* start-end indexes of { and } */

    if (cinfo.not_cluster) return 1;

    for (s = 0; s < keylen; s++)
        if (key[s] == '{') break;

    /* No '{' ? Hash the whole key. This is the base case. */
    if (s == keylen) return rw_crc16_ccitt_zero(key,keylen) & 0x3FFF;

    /* '{' found? Check if we have the corresponding '}'. */
    for (e = s+1; e < keylen; e++)
        if (key[e] == '}') break;

    /* No '}' or nothing betweeen {} ? Hash the whole key. */
    if (e == keylen || e == s+1) return rw_crc16_ccitt_zero(key,keylen) & 0x3FFF;

    /* If we are here there is both a { and a } on its right. Hash
     * what is in the middle between { and }. */
    return rw_crc16_ccitt_zero(key+s+1,e-s-1) & 0x3FFF;

}

static uint8_t rwdts_hash_slot_to_node_idx(rwdts_redis_instance_t *instance,
                                           uint16_t hashslot)
{
  int i = 0;

  RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
  for( i = 0; i < instance->cluster.count; ++i) {
    if (hashslot >= instance->cluster.nodes[i]->hash_start &&
        hashslot <= instance->cluster.nodes[i]->hash_last) {
      return instance->cluster.nodes[i]->node_idx;
    }
  }
  RW_ASSERT(!"I shouldn't be here!!!\n");
  return -1;
}

void rwdts_print_cluster_info_for_key(rwdts_redis_instance_t *instance,
                                      char *key,
                                      int len)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t hash = rwdts_key_hash_slot(key, len);
   int idx = rwdts_hash_slot_to_node_idx(instance, hash);

   RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
   node = instance->cluster.nodes[idx];
   fprintf(stdout, "Node info for slot[%d]:\n" \
              "Key = %s\n"
              "Slot idx = %d\n" \
              "Hash start = %d\n" \
              "Hash end = %d\n" \
              "host name = %s\n" \
              "port = %d\n", hash, key, idx, node->hash_start, node->hash_last, node->hostname, node->port);
}

void rwdts_redis_cmd(rwdts_redis_instance_t *instance,
                     int db_num, char *cmd, char *key, int key_len,
                     void *value, int value_len,
                     rwdts_trans_callback callback,
                     void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t hash = 0;
   int idx = 0;

   RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
   if (!cinfo.not_cluster) {
     hash = rwdts_key_hash_slot(key, key_len);
     idx = rwdts_hash_slot_to_node_idx(instance, hash);
   }
   node = instance->cluster.nodes[idx];
   int val = 0;

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
   RW_ASSERT((0 <= db_num) && (16 >= db_num));
   if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
     rwdts_connect_cluster_node(node);
   }
   RW_ASSERT(node->ctxt);

   RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
   val = redisAppendCommand(node->ctxt, "SELECT %d", db_num);
   if(!strcmp(cmd, "SET")) {
     val = redisAppendCommand(node->ctxt, "SET %b %b", key, key_len, value, value_len);
   } else if(!strcmp(cmd, "GET")) {
     val = redisAppendCommand(node->ctxt, "%s %b", cmd, key, (size_t)key_len);
   } else if(!strcmp(cmd, "DEL")) {
     val = redisAppendCommand(node->ctxt, "%s %b", cmd, key, (size_t)key_len);
   } else if(!strcmp(cmd, "EXISTS")) {
     val = redisAppendCommand(node->ctxt, "%s %b", cmd,  key, (size_t)key_len);
   }

   if (val != REDIS_OK) {
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              cmd, node->hostname, node->port);
     return;
   }
   if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
     int len = 0;
     do {
       val = redisBufferWrite(node->ctxt, &len);
       if (REDIS_ERR == val) break;
       RWDTS_REDIS_INCREMENT_STATS(node, tx);
     } while (!len);
   }
}

rw_status_t rwdts_redis_run_single_command(rwdts_redis_instance_t *instance,
                                           char *command, const char *sha_script, 
                                           char *key, int key_len, int db_num, char *value,
                                           int val_len, int serial_num, 
                                           rwdts_trans_callback callback,
                                           void *userdata)
{
  rwdts_redis_node_t *node = NULL;
  uint16_t hash = 0, val = 0;
  int idx = 0;

  if (!cinfo.not_cluster) {
    hash = rwdts_key_hash_slot(key, key_len);
    idx = rwdts_hash_slot_to_node_idx(instance, hash);
  }
  node = instance->cluster.nodes[idx];

  RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
  if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
    rwdts_connect_cluster_node(node);
  }
  RW_ASSERT(node->ctxt);

  RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
  if (!strcmp(command, "SET")) {
    val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %b %s %d %d %b", sha_script,
                             1, key, key_len, command, db_num, serial_num, value, val_len);
  } else {
    val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %b %s %d %d", sha_script,
                             1, key, key_len, command, db_num, serial_num);
  }

  if (val != REDIS_OK) { 
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "run script", node->hostname, node->port);
     return RW_STATUS_FAILURE;
  }

  if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
    int len = 0;
    do {
      val = redisBufferWrite(node->ctxt, &len);
      if (REDIS_ERR == val) return RW_STATUS_FAILURE;
      RWDTS_REDIS_INCREMENT_STATS(node, tx);
    } while (!len); 
  }
  return RW_STATUS_SUCCESS;
}

void rwdts_redis_hash_cmd(rwdts_redis_instance_t *instance,
                          int db_num, char *cmd, char *key,
                          int key_len, void *value,
                          int value_len, rwdts_trans_callback callback,
                          void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t hash = 0;
   int idx = 0;
   int serial_num = 0;

   RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
   if (!cinfo.not_cluster) {
     hash = rwdts_key_hash_slot(key, key_len);
     idx = rwdts_hash_slot_to_node_idx(instance, hash);
   }
   node = instance->cluster.nodes[idx];
   int val = 0;

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
   RW_ASSERT((0 <= db_num) && (16 >= db_num));
   if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
     rwdts_connect_cluster_node(node);
   }
   RW_ASSERT(node->ctxt);

   RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
   val = redisAppendCommand(node->ctxt, "SELECT %d", db_num);
   if (!strcmp(cmd, "HVALS")) {
     val = redisAppendCommand(node->ctxt, "HVALS %b", key, (size_t)key_len);
   } else if(!strcmp(cmd, "HGET")) {
       val = redisAppendCommand(node->ctxt, "HGET %s %d", key, serial_num);
   } else if (!strcmp(cmd, "HLEN")) {
     val = redisAppendCommand(node->ctxt, "HLEN %s", key);
   } else if(!strcmp(cmd, "HDEL")) {
       val = redisAppendCommand(node->ctxt, "HDEL %s %d", key, serial_num);
   } else if(!strcmp(cmd, "HGETALL")) {
     val = redisAppendCommand(node->ctxt, "HGETALL %b", key, (size_t)key_len);
   } else if(!strcmp(cmd, "HSETNX")) {
     val = redisAppendCommand(node->ctxt, "HSETNX %s %d %b", key,
                              serial_num, value, (size_t)value_len);
   } else if(!strcmp(cmd, "HEXISTS")) {
       val = redisAppendCommand(node->ctxt, "HEXISTS %s %d", key, serial_num);
   }
   if (val != REDIS_OK) {
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              cmd, node->hostname, node->port);
     return;
   }
   if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
     int len = 0;
     do {
       val = redisBufferWrite(node->ctxt, &len);
       if (REDIS_ERR == val) break;
       RWDTS_REDIS_INCREMENT_STATS(node, tx);
     } while (!len);
   }
}

void rwdts_redis_instance_deinit(rwdts_redis_instance_t *instance)
{
    RW_ASSERT(instance);
    int i = 0;
    rwdts_redis_node_t *node = NULL;

    RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
    for ( i = 0; i < instance->cluster.count; ++i) {
      node = instance->cluster.nodes[i];
      if (node->source) {
        rwsched_dispatch_source_cancel(instance->tasklet_info,
                                       node->source);
        node->source = NULL;
      }
      if (node->ctxt) {
        redisFree(node->ctxt);
        node->ctxt = NULL;
      }
      RWDTS_FLUSH_REDIS_HANDLE(node);
    }

    instance->magic = 0xDEAD;
    instance->sched_instance = NULL;
    instance->tasklet_info = NULL;
    instance->rwq = NULL;
    instance->callback_fn = NULL;
    instance->primary_node = NULL;
    instance->userdata = NULL;

    RW_FREE_TYPE(instance, rwdts_redis_instance_t);
}

int rwdts_redis_load_script(rwdts_redis_instance_t *instance,
                            const char *filename, rwdts_trans_callback callback,
                            void *userdata)
{
  sds script = sdsempty();
  FILE* fd = NULL;
  char buf[1024];
  size_t nread;
  int i = 0, val = 0, num_nodes = 1;
  rwdts_redis_node_t *node = NULL;

  RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);

  if (!cinfo.not_cluster) {
    num_nodes = instance->cluster.count;
  }

  fd = fopen(filename,"r");

  if (!fd) {
    return 0;
  }

  while((nread = fread(buf,1,sizeof(buf),fd)) != 0) {
      script = sdscatlen(script,buf,nread);
  }

  for (i = 0; i < num_nodes; i++) {
     node = instance->cluster.nodes[i];
    if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
      rwdts_connect_cluster_node(node);
    }
    RW_ASSERT(node->ctxt);
    RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
    val = redisAppendCommand(node->ctxt, "SCRIPT LOAD %s", script);
    if (val != REDIS_OK) {
      fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
               "script load", node->hostname, node->port);
      return 0;
    }

    if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
      int len = 0;
      do {
        val = redisBufferWrite(node->ctxt, &len);
        if (REDIS_ERR == val) break;
        RWDTS_REDIS_INCREMENT_STATS(node, tx);
      } while (!len);
    }
  }
  sdsfree(script);
  return num_nodes;
}

int rwdts_redis_run_get_next_script(rwdts_redis_instance_t *instance,
                                    const char *sha_script, int db_num,
                                    uint32_t *scan_list, char *shard_num,
                                    rwdts_trans_callback callback,
                                    void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t val = 0;
   int i = 0, num_nodes = 1;

   if (!cinfo.not_cluster) {
     num_nodes = instance->cluster.count;
   }

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);

   for (i = 0; i < num_nodes; i++) {
     int scan = 0;
     node = instance->cluster.nodes[i];
     if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
       rwdts_connect_cluster_node(node);
     }
     RW_ASSERT(node->ctxt);

     if (scan_list != NULL) {
       scan = scan_list[node->node_idx];
     }

     RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
     val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %d %d %s %d", sha_script,
                              0, db_num, scan, shard_num, node->node_idx);

     if (val != REDIS_OK) {
       fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "run script", node->hostname, node->port);
       return 0;
     }

     if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
       int len = 0;
       do {
         val = redisBufferWrite(node->ctxt, &len);
         if (REDIS_ERR == val) break;
         RWDTS_REDIS_INCREMENT_STATS(node, tx);
       } while (!len);
     }
   }
  return num_nodes;
}

rw_status_t rwdts_redis_run_set_script(rwdts_redis_instance_t *instance,
                                       const char *sha_script, char *key,
                                       int key_len, int db_num, int serial_num,
                                       char *shard_num,  char *value, int val_len,
                                       rwdts_trans_callback callback,
                                       void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t hash = 0, val = 0;
   int idx = 0;

   if (!cinfo.not_cluster) {
     hash = rwdts_key_hash_slot(key, key_len);
     idx = rwdts_hash_slot_to_node_idx(instance, hash);
   }
   node = instance->cluster.nodes[idx];

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
   if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
     rwdts_connect_cluster_node(node);
   }
   RW_ASSERT(node->ctxt);

   RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
   val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %b %d %d %s %b", sha_script,
                            1, key, key_len, db_num, serial_num, shard_num, value, (size_t)val_len);

  if (val != REDIS_OK) {
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "run script", node->hostname, node->port);
     return RW_STATUS_FAILURE;
  }

  if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
    int len = 0;
    do {
      val = redisBufferWrite(node->ctxt, &len);
      if (REDIS_ERR == val) return RW_STATUS_FAILURE;
      RWDTS_REDIS_INCREMENT_STATS(node, tx);
    } while (!len);
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_redis_run_shash_set_script(rwdts_redis_instance_t *instance,
                                 const char *sha_script, char *key, 
                                 int key_len, int db_num, int serial_num,
                                 char *shard_num,  char *value, int val_len,
                                 rwdts_trans_callback callback,
                                 void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t hash = 0, val = 0;
   int idx = 0;
   size_t shard_len = (size_t)strlen(shard_num);

   if (!cinfo.not_cluster) {
     hash = rwdts_key_hash_slot(shard_num, shard_len); 
     idx = rwdts_hash_slot_to_node_idx(instance, hash);
   }
   node = instance->cluster.nodes[idx];

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
   if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
     rwdts_connect_cluster_node(node);
   }
   RW_ASSERT(node->ctxt);

   RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
   val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %s %b %d %d %b", sha_script,
                            1, shard_num, key, (size_t)key_len, db_num, serial_num, value, (size_t)val_len);

  if (val != REDIS_OK) {
     RW_ASSERT(0);
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "run shash script", node->hostname, node->port);
     return RW_STATUS_FAILURE;
  }

  if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
    int len = 0;
    do {
      val = redisBufferWrite(node->ctxt, &len);
      if (REDIS_ERR == val) return RW_STATUS_FAILURE;
      RWDTS_REDIS_INCREMENT_STATS(node, tx);
    } while (!len);
  }
  return RW_STATUS_SUCCESS;
}

void rwdts_redis_run_del_script(rwdts_redis_instance_t *instance,
                                const char *sha_script, char *key,
                                int key_len, int db_num, int serial_num,
                                rwdts_trans_callback callback,
                                void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t hash = 0, val = 0;
   int idx = 0;

   if (!cinfo.not_cluster) {
     hash = rwdts_key_hash_slot(key, key_len);
     idx = rwdts_hash_slot_to_node_idx(instance, hash);
   }
   node = instance->cluster.nodes[idx];

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
   if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
     rwdts_connect_cluster_node(node);
   }
   RW_ASSERT(node->ctxt);

   RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
   val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %b %d %d", sha_script,
                            1, key, key_len, db_num, serial_num);

  if (val != REDIS_OK) {
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "run script", node->hostname, node->port);
     return;
  }

  if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
    int len = 0;
    do {
      val = redisBufferWrite(node->ctxt, &len);
      if (REDIS_ERR == val) break;
      RWDTS_REDIS_INCREMENT_STATS(node, tx);
    } while (!len);
  }
  return;
}

int rwdts_redis_del_table(rwdts_redis_instance_t *instance, int db_num,
                          rwdts_trans_callback callback, void *userdata)
{
  int i = 0, val = 0, num_nodes = 1;
  rwdts_redis_node_t *node = NULL;

  RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);

  if (!cinfo.not_cluster) {
    num_nodes = instance->cluster.count;
  }

  for (i = 0; i < num_nodes; i++) {
     node = instance->cluster.nodes[i];
    if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
      rwdts_connect_cluster_node(node);
    }
    RW_ASSERT(node->ctxt);
    RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
    val = redisAppendCommand(node->ctxt, "SELECT %d", db_num);
    val = redisAppendCommand(node->ctxt, "FLUSHDB");
    if (val != REDIS_OK) {
      fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
               "script load", node->hostname, node->port);
      return 0;
    }

    if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
      int len = 0;
      do {
        val = redisBufferWrite(node->ctxt, &len);
        if (REDIS_ERR == val) break;
        RWDTS_REDIS_INCREMENT_STATS(node, tx);
      } while (!len);
    }
  }
  return num_nodes;
}

int rwdts_redis_run_get_script(rwdts_redis_instance_t *instance,
                               const char *sha_script, int db_num,
                               rwdts_trans_callback callback,
                               void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t val = 0;
   int i = 0, num_nodes = 1;

   if (!cinfo.not_cluster) {
     num_nodes = instance->cluster.count;
   }

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);

   for (i = 0; i < num_nodes; i++) {
     node = instance->cluster.nodes[i];
     if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
       rwdts_connect_cluster_node(node);
     }
     RW_ASSERT(node->ctxt);

     RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
     val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %d", sha_script,
                              0, db_num);

     if (val != REDIS_OK) {
       fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "run script", node->hostname, node->port);
       return 0;
     }

     if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
       int len = 0;
       do {
         val = redisBufferWrite(node->ctxt, &len);
         if (REDIS_ERR == val) break;
         RWDTS_REDIS_INCREMENT_STATS(node, tx);
       } while (!len);
     }
   }
  return num_nodes;
}

int rwdts_redis_run_get_all_shash_script(rwdts_redis_instance_t *instance,
                                         const char *sha_script, int db_num,
                                         char *shard_num, rwdts_trans_callback callback,
                                         void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t val = 0;
   int i = 0, num_nodes = 1, sent_count = 0;

   if (!cinfo.not_cluster) {
     num_nodes = instance->cluster.count;
   }

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);

   for (i = 0; i < num_nodes; i++) {
     node = instance->cluster.nodes[i];
     if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
       rwdts_connect_cluster_node(node);
     }
     RW_ASSERT(node->ctxt);

     RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
     val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %d %s", sha_script,
                              0, db_num, shard_num);

     if (val != REDIS_OK) {
       fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "get all script", node->hostname, node->port);
       return 0;
     }

     if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
       int len = 0;
       do {
         val = redisBufferWrite(node->ctxt, &len);
         if (REDIS_ERR == val) break;
         RWDTS_REDIS_INCREMENT_STATS(node, tx);
       } while (!len);
       if (val != REDIS_ERR) {
         sent_count++;
       }
     }
   }
  return sent_count;
}

rw_status_t rwdts_redis_run_del_shash_script(rwdts_redis_instance_t *instance,
                                             const char *sha_script, int db_num,
                                             char *shard_num, char *key, int key_len,
                                             rwdts_trans_callback callback, void *userdata)
{
  rwdts_redis_node_t *node = NULL;
  uint16_t hash = 0, val = 0;
  rw_status_t status = RW_STATUS_FAILURE;
  int idx = 0;
  size_t shard_len = strlen(shard_num);

  if (!cinfo.not_cluster) {
    hash = rwdts_key_hash_slot(shard_num, shard_len); 
    idx = rwdts_hash_slot_to_node_idx(instance, hash);
  }
  node = instance->cluster.nodes[idx];

  RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
  if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
    rwdts_connect_cluster_node(node);
  }
  RW_ASSERT(node->ctxt);

  RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);

  RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
  val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %s %b %d", sha_script,
                           1, shard_num, key, (size_t)key_len, db_num);

  if (val != REDIS_OK) {
    fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
           "del shash script", node->hostname, node->port);
    return RW_STATUS_FAILURE;
  }

  if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
    int len = 0;
    do {
      val = redisBufferWrite(node->ctxt, &len);
      if (REDIS_ERR == val) break;
      RWDTS_REDIS_INCREMENT_STATS(node, tx);
    } while (!len);
    if (val != REDIS_ERR) {
      status = RW_STATUS_SUCCESS;
    }
  }
  return status;
}

int rwdts_redis_run_del_shard_script(rwdts_redis_instance_t *instance,
                                     const char *sha_script, int db_num,
                                     char *shard_num, rwdts_trans_callback callback,
                                     void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t val = 0;
   int i = 0, num_nodes = 1;

   if (!cinfo.not_cluster) {
     num_nodes = instance->cluster.count;
   }

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);

   for (i = 0; i < num_nodes; i++) {
     node = instance->cluster.nodes[i];
     if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
       rwdts_connect_cluster_node(node);
     }
     RW_ASSERT(node->ctxt);

     RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
     val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %d %s", sha_script,
                              0, db_num, shard_num);

     if (val != REDIS_OK) {
       fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "run script", node->hostname, node->port);
       return 0;
     }

     if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
       int len = 0;
       do {
         val = redisBufferWrite(node->ctxt, &len);
         if (REDIS_ERR == val) break;
         RWDTS_REDIS_INCREMENT_STATS(node, tx);
       } while (!len);
     }
   }
  return num_nodes;
}

void rwdts_redis_run_set_pend_commit_abort_script(rwdts_redis_instance_t *instance,
                                                  const char *sha_script, char *key,
                                                  int key_len, int db_num, int serial_num,
                                                  char *shard_num, rwdts_trans_callback callback,
                                                  void *userdata)
{
   rwdts_redis_node_t *node = NULL;
   uint16_t hash = 0, val = 0;
   int idx = 0;

   if (!cinfo.not_cluster) {
     hash = rwdts_key_hash_slot(key, key_len);
     idx = rwdts_hash_slot_to_node_idx(instance, hash);
   }
   node = instance->cluster.nodes[idx];

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
   if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
     rwdts_connect_cluster_node(node);
   }
   RW_ASSERT(node->ctxt);

   RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
   val = redisAppendCommand(node->ctxt, "EVALSHA %s %d %b %d %d %s", sha_script,
                            1, key, key_len, db_num, serial_num, shard_num);

  if (val != REDIS_OK) {
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "run script", node->hostname, node->port);
     return;
  }

  if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
    int len = 0;
    do {
      val = redisBufferWrite(node->ctxt, &len);
      if (REDIS_ERR == val) break;
      RWDTS_REDIS_INCREMENT_STATS(node, tx);
    } while (!len);
  }
  return;
}

void rwdts_redis_slave_to_master(rwdts_redis_instance_t *instance,
                                 rwdts_trans_callback callback,
                                 void *userdata)
{
   rwdts_redis_node_t *node = NULL;

   RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
   node = instance->cluster.nodes[0];
   int val = 0;

   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
   if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
     rwdts_connect_cluster_node(node);
   }
   RW_ASSERT(node->ctxt);

   RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
   val = redisAppendCommand(node->ctxt, "SLAVEOF NO ONE");

   if (val != REDIS_OK) {
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "SLAVEOF NO ONE", node->hostname, node->port);
     return;
   }
   if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
     int len = 0;
     do {
       val = redisBufferWrite(node->ctxt, &len);
       if (REDIS_ERR == val) break;
       RWDTS_REDIS_INCREMENT_STATS(node, tx);
     } while (!len);
   }
}

void rwdts_redis_master_to_slave(rwdts_redis_instance_t *instance,
                                 char *hostname, uint16_t port,
                                 rwdts_trans_callback callback,
                                 void *userdata)
{
   rwdts_redis_node_t *node = NULL;

   RW_ASSERT(RWDTS_REDIS_VALID_INSTANCE(instance));
   node = instance->cluster.nodes[0];
   int val = 0;
        
   RW_ASSERT(RWDTS_REDIS_STATE_CONNECTED == instance->state);
   if (RWDTS_REDIS_CLIENT_STATE_INVALID == node->state) {
     rwdts_connect_cluster_node(node);
   }
   RW_ASSERT(node->ctxt);

   RWDTS_CREATE_AND_ENQUEUE_HANDLE(node, callback, userdata);
   val = redisAppendCommand(node->ctxt, "SLAVEOF %s %d", hostname, (int)port);

   if (val != REDIS_OK) {
     fprintf(stdout, "Failed to append %s command for node[%s:%d]\n",
              "SLAVEOF <master-ip-adress> <master-port>", node->hostname, node->port);
     return;
   }
   if (RWDTS_REDIS_CLIENT_STATE_CONNECTED == node->state) {
     int len = 0;
     do {
       val = redisBufferWrite(node->ctxt, &len);
       if (REDIS_ERR == val) break;
       RWDTS_REDIS_INCREMENT_STATS(node, tx);
     } while (!len);
   }
}

