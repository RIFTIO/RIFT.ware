
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwdts_redis_client.c
 * @author Vijay Nag (Vijay.Nag@riftio.com)
 * @date 2014/10/24
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

struct {
  rwdts_redis_instance_t *instance;
  rwsched_instance_ptr_t rwsched;
  rwsched_tasklet_ptr_t tasklet;
  rwsched_dispatch_source_t source;
  long long timeout;
} myUserData;

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

struct abc {
  int a;
  int b;
  int c;
} a;


static rwdts_reply_status redis_del_callbak(rwdts_redis_msg_handle *handle, void *userdata)
{
  if(RW_HANDLE_STATE_REPLY == handle->status) {
    if (RWDTS_DEL_SUCCESSFUL(handle)) {
      fprintf(stderr, "Successfully deleted the data\n");
    } else {
      fprintf(stderr, "Failed to delete the data\n");
    }
  }
  return RWDTS_REDIS_REPLY_DONE;
}


static rwdts_reply_status redis_get_callback(rwdts_redis_msg_handle *handle, void *userdata)
{
   struct abc *reply = (struct abc*)RWDTS_VIEW_REPLY(handle);
   rwdts_redis_instance_t *redis_instance = (rwdts_redis_instance_t *)userdata;

   if(RW_HANDLE_STATE_REPLY == handle->status) {
     if ((reply->a == a.a) && (reply->b == a.b) && (reply->c == a.c)) {
      fprintf(stderr, "Successfully extracted data from database\n");
      fprintf(stderr, "a->a(%d), a->b(%d), a->c(%d)\n", reply->a, reply->b, reply->c);
     }
     rwdts_redis_del_key_value(redis_instance, 1, "FOO", 3,
                               redis_del_callbak, NULL);
   }
   return RWDTS_REDIS_REPLY_DONE;
}

static rwdts_reply_status redis_set_callback(rwdts_redis_msg_handle *handle, void *userdata)
{
  rwdts_redis_instance_t *redis_instance = (rwdts_redis_instance_t *)userdata;
  if(RW_HANDLE_STATE_REPLY == handle->status) {
    fprintf(stderr, "Successfully inserted the data\n");
  }

  rwdts_redis_get_key_value(redis_instance, 1, "FOO", 3,
                            redis_get_callback, redis_instance);
  return RWDTS_REDIS_REPLY_DONE;
}

void redis_initialized(rwdts_redis_db_init_info *db_info)
{
  if (RWDTS_REDIS_DB_UP == db_info->status) {
    rwdts_redis_instance_t *redis_instance = db_info->instance;
    fprintf(stderr, "[%s]Riftdb is up\n", getTime());
    a.a = 1;
    a.b = 2;
    a.c = 3;

    rwdts_redis_set_key_value(redis_instance, 1, "FOO", 3,
                              (void*)&a,
                              sizeof(a),
                              redis_set_callback, (void*)redis_instance);
  } else {
    fprintf(stderr, "Err connecting to Redis db[%s]\n",
             RWDTS_REDIS_DB_ERR_STR(db_info));
  }
}

int main(int argc, char **argv, char **envp)
{
  rw_status_t status;
  uint16_t port = atoi(argv[2]);
  rwsched_instance_ptr_t rwsched = NULL;

  if (3 != argc) {
    fprintf(stderr, "Usage: ./rwdts_redis_client <ip> <port>\n");
    return -1;
  }
  rwsched = rwsched_instance_new();
  RW_ASSERT(rwsched);

  rwsched_tasklet_ptr_t tasklet = NULL;
  tasklet = rwsched_tasklet_new(rwsched);
  RW_ASSERT(tasklet);

  status = rwdts_redis_instance_init(rwsched, tasklet, argv[1], port, redis_initialized, NULL);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  myUserData.instance = NULL;
  myUserData.rwsched = rwsched;
  myUserData.tasklet = tasklet;

  rwsched_dispatch_main_until(tasklet, 1000, 0);
  return 1;
}

