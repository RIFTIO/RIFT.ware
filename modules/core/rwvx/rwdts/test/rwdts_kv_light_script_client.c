
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
 * @file rwdts_kv_light_client.c
 * @author Prashanth Bhaskar (Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 * @brief Do nothing test program to validate declarations, linkage, compile-time stuff
 */

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwmsg.h>
#include <rwdts_redis.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>

struct {
  rwdts_kv_handle_t *handle;
  rwdts_kv_table_handle_t *tab_handle;
  rwsched_instance_ptr_t rwsched;
  rwsched_tasklet_ptr_t tasklet;
  rwsched_dispatch_source_t source;
  int get_index;
  int count;
  long long timeout;
  int my_shard_deleted;
} myUserData;

char *key_entry[30] = {"name1", "name2", "name3", "name4", "name5", "name6", "name7",
                      "name8", "name9", "name10", "name11", "name12", "name13",
                      "name14", "name15", "name16", "name17", "name18", "name19"};
char *tab_entry[30] = {"TEST", "Sheldon", "Leonard", "Raj", "Howard", "Bernie",
                       "Amy", "Steve", "Wilma", "Fred", "Charlie", "Penny","What",
                       "When", "Why", "Who", "How", "BBC", "CNN"};

char *getTime(void)
{
 struct timeval tv;
 struct tm* ptm;
 char time_string[40];
 long milliseconds;
 static char buf[4096];

 gettimeofday (&tv, NULL);
 ptm = localtime (&tv.tv_sec);
 strftime (time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
 milliseconds = tv.tv_usec / 1000;
 snprintf(buf, sizeof(buf), "%s.%03ld", time_string, milliseconds);
 return buf;
}


static rwdts_kv_light_reply_status_t rwdts_kv_light_get_all_callback(void **key,
                                                                     int *keylen,
                                                                     void **value,
                                                                     int *val_len,
                                                                     int total,
                                                                     void *userdata);

static rwdts_kv_light_reply_status_t rwdts_kv_light_delete_shard_cbk_fn(rwdts_kv_light_del_status_t status,
                                                                        void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  if(RWDTS_KV_LIGHT_DEL_SUCCESS == status) {
    fprintf(stderr, "Successfully deleted the data\n");
  }
  myUserData.my_shard_deleted = 1;
  rwdts_kv_handle_get_all(tab_handle->handle, tab_handle->db_num, rwdts_kv_light_get_all_callback, (void *)tab_handle);
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_all_callback(void **key,
                                                                     int *keylen,
                                                                     void **value,
                                                                     int *val_len,
                                                                     int total,
                                                                     void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  int i = 0;

  for (i = 0; i < total; i++) {
    fprintf(stderr, "%s\n", (char *)value[i]);
    fprintf(stderr, "%s\n", (char *)key[i]);
    RW_FREE(value[i]);
    RW_FREE(key[i]);
  }

  if (!myUserData.my_shard_deleted) {
    rwdts_kv_light_delete_shard_entries(tab_handle, "What",
                                        rwdts_kv_light_delete_shard_cbk_fn,
                                        (void *)tab_handle);
  }
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_scr_run_callback(void **val,
                                                                     int *val_len,
                                                                     void **key,
                                                                     int *key_len,
                                                                     int *serial_num,
                                                                     int total,
                                                                     void *userdata,
                                                                     void *next_block)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  int i = 0;

  myUserData.get_index += total;

  for (i = 0; i < total; i++) {
    fprintf(stderr, "%s\n", (char *)val[i]);
    fprintf(stderr, "%s\n", (char *)key[i]);
    RW_FREE(val[i]);
    RW_FREE(key[i]);
  }
  rwdts_kv_handle_get_all(tab_handle->handle, tab_handle->db_num, rwdts_kv_light_get_all_callback, (void *)tab_handle);
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_set_callback(rwdts_kv_light_set_status_t status,
                                                                 int serial_num, void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  if(RWDTS_KV_LIGHT_SET_SUCCESS == status) {
    fprintf(stderr, "Successfully inserted the data\n");
  }

  myUserData.count++;
  if (myUserData.count == 19) {
    rwdts_kv_light_get_next_fields(tab_handle, "When", rwdts_kv_light_scr_run_callback,
                                   (void*)tab_handle, NULL);
  }
  return RWDTS_KV_LIGHT_REPLY_DONE;
}


void redis_initialized(void *userdata, rw_status_t status)
{
  int i;
  char shard_num[100] = "When";
  /* ok, redis client connection to databases are up */
  fprintf(stderr, "[%s]Riftdb is up\n", getTime());
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  myUserData.tab_handle = rwdts_kv_light_register_table(myUserData.handle, 0);
  for (i = 0; i< 19; i++) {
    if (i > 13) {
      strcpy(shard_num, "What");
    }
    rwdts_kv_light_table_insert(myUserData.tab_handle, (i+1), shard_num, key_entry[i],
                                strlen(key_entry[i]), tab_entry[i], strlen(tab_entry[i]),
                                rwdts_kv_light_set_callback, (void*)myUserData.tab_handle);
  }
}

int main(int argc, char **argv, char **envp) {

  if (3 != argc) {
    fprintf(stderr, "Usage: ./rwdts_redis_client <ip> <port>\n");
    return -1;
  }

  //uint16_t port = atoi(argv[2]);

  myUserData.handle = rwdts_kv_allocate_handle(REDIS_DB);
  rwsched_instance_ptr_t rwsched = NULL;
  rwsched = rwsched_instance_new();
  RW_ASSERT(rwsched);

  rwsched_tasklet_ptr_t tasklet = NULL;
  tasklet = rwsched_tasklet_new(rwsched);
  RW_ASSERT(tasklet);

  rwdts_kv_handle_db_connect(myUserData.handle, rwsched, tasklet, "127.0.0.1:9997",
                             "test", NULL, redis_initialized, myUserData.handle);

  myUserData.rwsched = rwsched;
  myUserData.tasklet = tasklet;
  myUserData.count = 0;
  myUserData.my_shard_deleted = 0;

  rwsched_dispatch_main_until(tasklet, 1000, 0);

  return 1;
}
