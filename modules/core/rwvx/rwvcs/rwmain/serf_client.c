
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwvcs_serf_rpc_client.c
 * @author Shaji Radhakrishnan (shaji.radhakrishnan@riftio.com)
 * @date 12/01/2014
 * @brief Serf RPC client connect program
 *
 */


#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>

// CF must be first -- RIFT-4327
#include <CoreFoundation/CoreFoundation.h>
#include <msgpack.h>
#include <msgpack/object.h>

#include <rw-log.pb-c.h>
#include <rw-vm-heartbeat-log.pb-c.h>
#include <rwlib.h>
#include <rwsched_main.h>
#include <rwsched/cfsocket.h>
#include <rwsched/cfrunloop.h>
#include <rwvcs.h>

#include "serf_client.h"

serf_log_level_str_t serf_log_str[]= {
  {RW_LOG_LOG_SEVERITY_EMERGENCY, "ERRO"},
  {RW_LOG_LOG_SEVERITY_ALERT, "ERRO"},
  {RW_LOG_LOG_SEVERITY_CRITICAL, "ERRO"},
  {RW_LOG_LOG_SEVERITY_ERROR, "ERRO"},
  {RW_LOG_LOG_SEVERITY_WARNING, "WARN"},
  {RW_LOG_LOG_SEVERITY_NOTICE, "INFO"},
  {RW_LOG_LOG_SEVERITY_INFO, "INFO"},
  {RW_LOG_LOG_SEVERITY_DEBUG, "DEBUG"}
};

struct serf_connect_closure {
  serf_context_ptr_t serf;
  double interval;
  uint32_t attempts;
  uint32_t max_attempts;
};

/*
 * Send a command to the SERF server.
 *
 * @param serf    - serf context
 * @param command - command to send in header
 * @param seq     - sequence number to use in header
 * @param format  - format of following arguments if any which will be sent as
 *                  the command body.
 *
 *                  'u':  uint32_t
 *                  's':  char *
 * @return        - rw_status_t
 */
rw_status_t serf_send_command(
    serf_context_ptr_t serf,
    const char * command,
    uint32_t seq,
    const char * format,
    ...);

/*
 * Return a string that is properly terminated as msgpack treats strings as
 * binary data.  Note that the caller will be required to free the returned
 * string
 *
 * @param msg - A message pack object containing a string.
 * @return    - A newly allocated string containing a null-terminated copy
 *              of the string contained in the specified message.  It is up
 *              to the caller to free this string.  If the message did not
 *              contain a string, NULL will be returned.
 */
static inline char * get_string(msgpack_object * msg)
{
  if (msg->type != MSGPACK_OBJECT_STR)
    return NULL;

  char * p = (char *)malloc((msg->via.str.size + 1) * sizeof(char));
  memcpy(p, msg->via.str.ptr, msg->via.str.size);
  p[msg->via.str.size] = '\0';
  return p;
}

/*
 * Free a serf_member.
 */
static void free_serf_member(struct serf_member * member)
{
  if (member->name)
    free(member->name);
  if (member->status)
    free(member->status);

  free(member);
}

/*
 * Parse member data from a msgpack object
 *
 * @param msg - message data object containing member information
 * @return    - newly allocated serf_member structure or NULL
 */
static struct serf_member * parse_serf_member(msgpack_object * msg)
{
  struct serf_member * member;

  if (msg->type != MSGPACK_OBJECT_MAP)
    return NULL;

  member = (struct serf_member *)malloc(sizeof(struct serf_member));
  RW_ASSERT(member);
  bzero(member, sizeof(struct serf_member));

  for (int i = 0; i < msg->via.map.size; i++) {
    const char * key;
    msgpack_object * val;

    if (msg->via.map.ptr[i].key.type != MSGPACK_OBJECT_STR)
      continue;

    key = msg->via.map.ptr[i].key.via.str.ptr;
    val = &(msg->via.map.ptr[i].val);

    if (!strncmp(key, "Name", 4)) {
      member->name = get_string(val);
    } else if (!strncmp(key, "Status", 6)) {
      member->status = get_string(val);
    } else if (!strncmp(key, "Addr", 4)) {
      snprintf(
          member->address,
          sizeof(member->address),
          "%d.%d.%d.%d",
          (int)val->via.bin.ptr[0],
          (int)val->via.bin.ptr[1],
          (int)val->via.bin.ptr[2],
          (int)val->via.bin.ptr[3]);
    }
  }

  if (!member->name || !member->status || !member->address[0]) {
    free_serf_member(member);
    return NULL;
  }

  return member;
}

/*
 * serf_cleanup_instance - This function cleanup the serf memory and other
 * stuffs
 * @param - serf - serf instance memory ptr
 */
static void
serf_cleanup_instance(
    serf_context_ptr_t serf);

/*
 * Get the sequence number from a header message.  If the message is not a SERF
 * RPC header then 0 will be returned.
 *
 * @param msg - message to extrace sequence number from
 * @return    - The sequence number or 0 if the message is not a SERF RPC header.
 */
static inline uint32_t get_header_seq(msgpack_object * msg) {
  if (msg->type != MSGPACK_OBJECT_MAP)
    return 0;

  if (msg->via.map.size != 2)
    return 0;

  for (int i = 0; i < 2; i++) {
    msgpack_object *key = &(msg->via.map.ptr[i].key);
    msgpack_object *val = &(msg->via.map.ptr[i].val);

    if (key->type != MSGPACK_OBJECT_STR)
      continue;

    if (key->via.str.size != 3 || strncmp(key->via.str.ptr, "Seq", 3))
      continue;

    return val->via.u64;
  }

  return 0;
}

serf_context_ptr_t
serf_instance_alloc(
    rwvcs_instance_ptr_t rwvcs_instance,
    char *rpc_ip_address,
    unsigned short rpc_port)
{
  serf_context_ptr_t serf = malloc(sizeof(struct serf_context));

  if (serf == NULL) {
    return NULL;
  }
  memset(serf, 0, sizeof(struct serf_context));

  serf->state = SERF_STATE_CLOSED;
  serf->mpac = msgpack_unpacker_new(1024);
  serf->rwvcs_instance = rwvcs_instance;
  serf->rwsched = rwvcs_instance->rwvx->rwsched;
  serf->rwsched_tasklet = rwvcs_instance->rwvx->rwsched_tasklet;
  serf->rpc_port = rpc_port;
  serf->log_level =  RW_LOG_LOG_SEVERITY_INFO;

  if (rpc_ip_address) {
    strncpy(serf->rpc_ip_address,
            rpc_ip_address,
            sizeof(serf->rpc_ip_address));
  } else {
    //FIXME return error
    RWLOG_EVENT(serf->rwvcs_instance->rwvx->rwlog, RwVmHeartbeatLog_notif_MiscError,
                "serf agent rpc address is not received", 0);

    strncpy(serf->rpc_ip_address,
            "127.0.0.1",
            sizeof(serf->rpc_ip_address));
  }
  return((void *)serf);
}

static bool handle_handshake_response(serf_context_ptr_t serf)
{
  rw_status_t status;

  if (serf->state != SERF_STATE_HANDSHAKE_SENT)
    return false;

  serf->state = SERF_STATE_STREAM_SENT;
  status = serf_send_command(serf, "stream", ++serf->seqno, "ss", "Type", "*");
  serf->stream_seq = serf->seqno;

  if (status != RW_STATUS_SUCCESS) {
    RWLOG_EVENT(
        serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "Failed to send stream request",
        0);
  }

  return true;
}

static bool handle_stream_response(serf_context_ptr_t serf, msgpack_object * msg)
{
  rw_status_t status;

  if (serf->state == SERF_STATE_STREAM_SENT) {
    serf->state = SERF_STATE_MONITOR_SENT;
    status = serf_send_command(
        serf,
        "monitor",
        ++serf->seqno,
        "ss",
        "LogLevel",
        serf_log_str[serf->log_level].serf_string);
    serf->monitor_seq = serf->seqno;

    if (status != RW_STATUS_SUCCESS) {
      RWLOG_EVENT(
          serf->rwvcs_instance->rwvx->rwlog,
          RwVmHeartbeatLog_notif_MiscError,
          "Failed to send monitor",
          0);
    }
    return true;
  } else if (msg) {
    char * event_str;
    static char log_string[512];
    msgpack_object * event = &(msg->via.map.ptr[0].val);
    msgpack_object * members = &(msg->via.map.ptr[1].val);

    if (!event->type == MSGPACK_OBJECT_STR
        || event->via.str.size < 7
        || strncmp("member-", event->via.str.ptr, 7))
      return false;

    event_str = get_string(event);
    for (int i = 0; i < members->via.array.size; ++i) {
      struct serf_event_callback * ec;
      struct serf_member * member;

      member = parse_serf_member(&(members->via.array.ptr[i]));
      if (!member)
        continue;

      snprintf(log_string, sizeof(log_string), "<%s:%d> Event %s (%d/%d) Member: %s at %s",
          serf->rpc_ip_address,
          serf->rpc_port,
          event_str,
          i,
          members->via.array.size,
          member->name,
          member->address);

      RWLOG_EVENT(
          serf->rwvcs_instance->rwvx->rwlog,
          RwVmHeartbeatLog_notif_TraceInfo,
          log_string);

      SLIST_FOREACH(ec, &(serf->event_cbs), _list) {
        ec->callback(ec->ctx, event_str, member);
      }

      free_serf_member(member);
    }

    free(event_str);

    return true;
  }

  return false;
}

static bool handle_monitor_response(serf_context_ptr_t serf, msgpack_object * msg)
{
  if (serf->state == SERF_STATE_MONITOR_SENT) {
    serf->state = SERF_STATE_ESTABLISHED;
    return true;
  } else if (msg) {
    char * val_str;
    msgpack_object * key = &(msg->via.map.ptr[0].key);
    msgpack_object * val = &(msg->via.map.ptr[0].val);

    if (!key->type == MSGPACK_OBJECT_STR
        || !val->type == MSGPACK_OBJECT_STR
        || key->via.str.size != 3
        || strncmp("Log", key->via.str.ptr, 3))
      return false;

    val_str = get_string(val);

    if (strstr(val->via.str.ptr, "[INFO]")) {
       RWLOG_EVENT(
           serf->rwvcs_instance->rwvx->rwlog,
           RwVmHeartbeatLog_notif_SerfMonitorInfo,
           val_str);
    } else if (strstr(val_str, "[WARN]")) {
      RWLOG_EVENT(
          serf->rwvcs_instance->rwvx->rwlog,
          RwVmHeartbeatLog_notif_SerfMonitorWarn,
          val_str);
    } else if (strstr(val_str, "[ERROR]")) {
      RWLOG_EVENT(
          serf->rwvcs_instance->rwvx->rwlog,
          RwVmHeartbeatLog_notif_SerfMonitorError,
          val_str);
    } else {
      RWLOG_EVENT(
          serf->rwvcs_instance->rwvx->rwlog,
          RwVmHeartbeatLog_notif_SerfMonitorDebug,
          val_str);
    }

    free(val_str);

    return true;
  }
  return false;
}

static bool handle_message_by_seq(serf_context_ptr_t serf, uint32_t seq, msgpack_object * msg)
{
  if (seq == serf->handshake_seq) {
    return handle_handshake_response(serf);
  } else if (seq == serf->stream_seq) {
    return handle_stream_response(serf, msg);
  } else if (seq == serf->monitor_seq) {
    return handle_monitor_response(serf, msg);
  } else {
    RWLOG_EVENT(
        serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "Unknown sequence number recieved",
        seq);
  }

  return false;
}

static rw_status_t serf_recv_and_process_events(serf_context_ptr_t serf)
{
  if (serf->sockfd == -1) {
    RWLOG_EVENT(
        serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "Serf RPC socket fd is invalid, Returning failure",
        EINVAL);
    return RW_STATUS_FAILURE;
  }

  while (1) {
    ssize_t read;
    size_t read_max;

    read_max = msgpack_unpacker_buffer_capacity(serf->mpac);
    if (!read_max) {
      msgpack_unpacker_expand_buffer(serf->mpac, serf->mpac->initial_buffer_size * 2);
      continue;
    }

    read = recv(serf->sockfd, msgpack_unpacker_buffer(serf->mpac), read_max, 0);
    if (read < 0) {
      if (errno != EAGAIN) {
        RWLOG_EVENT(
            serf->rwvcs_instance->rwvx->rwlog,
            RwVmHeartbeatLog_notif_MiscError,
            "Failed to receive data from socket",
            errno);
      }
      break;
    } else if (read == 0) {
      RWLOG_EVENT(
          serf->rwvcs_instance->rwvx->rwlog,
          RwVmHeartbeatLog_notif_MiscError,
          "SERF RPC socket closed",
          0);
      serf_cleanup_instance(serf);
      break;
    }

    msgpack_unpacker_buffer_consumed(serf->mpac, read);
    if (read == read_max) {
      msgpack_unpacker_expand_buffer(serf->mpac, serf->mpac->initial_buffer_size * 2);
      continue;
    }


    msgpack_unpack_return ret;
    msgpack_unpacked header;
    msgpack_unpacked msg;

    msgpack_unpacked_init(&msg);
    msgpack_unpacked_init(&header);

    ret = msgpack_unpacker_next(serf->mpac, &msg);
    while (ret == MSGPACK_UNPACK_SUCCESS) {
      uint32_t seq = get_header_seq(&msg.data);
      if (seq) {
        RW_ASSERT(!serf->last_seq);
        serf->last_seq = seq;
        if (handle_message_by_seq(serf, serf->last_seq, NULL))
          serf->last_seq = 0;
      } else {
        RW_ASSERT(serf->last_seq);
        if (handle_message_by_seq(serf, serf->last_seq, &msg.data))
          serf->last_seq = 0;
      }
      ret = msgpack_unpacker_next(serf->mpac, &msg);
    }

    msgpack_unpacked_destroy(&header);
    msgpack_unpacked_destroy(&msg);
  }

  return RW_STATUS_SUCCESS;
}


static void handle_new_msgs(
    rwsched_CFSocketRef s,
    CFSocketCallBackType type,
    CFDataRef address,
    const void *data,
    void *info)
{
  serf_context_ptr_t serf;

  serf = (serf_context_ptr_t)info;

  serf_recv_and_process_events(serf);
}

static void setup_cfsource(serf_context_ptr_t serf)
{
  CFSocketContext cf_context;
  rwsched_CFSocketRef cfsocket;
  rwsched_CFRunLoopSourceRef cfsource;

  bzero(&cf_context, sizeof(cf_context));
  cf_context.info = (void *)serf;

  cfsocket = rwsched_tasklet_CFSocketCreateWithNative(
      serf->rwsched_tasklet,
      kCFAllocatorSystemDefault,
      serf->sockfd,
      kCFSocketReadCallBack,
      handle_new_msgs,
      &cf_context);
  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);

  rwsched_tasklet_CFSocketSetSocketFlags(
      serf->rwsched_tasklet,
      cfsocket,
      kCFSocketAutomaticallyReenableReadCallBack);

  cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(
      serf->rwsched_tasklet,
      kCFAllocatorSystemDefault,
      cfsocket,
      0);
  RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);

  rwsched_tasklet_CFRunLoopAddSource(
      serf->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(serf->rwsched_tasklet),
      cfsource,
      rwsched_instance_CFRunLoopGetMainMode(serf->rwsched));

  serf->cfsource = cfsource;
  serf->cfsocket = cfsocket;
}

static void try_rpc_connect(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  rw_status_t status;
  struct serf_connect_closure * cls;
  struct sockaddr_in server;
  int optval;
  int flags;
  int r;

  cls = (struct serf_connect_closure *)ctx;

  cls->serf->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (cls->serf->sockfd == -1) {
    RWLOG_EVENT(
        cls->serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "Socket was not created",
        errno);
    goto done;
  }

  optval = 1;
  r = setsockopt(cls->serf->sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
  if (r) {
    RWLOG_EVENT(
        cls->serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "setsockopt failed",
        errno);
    goto close_socket;
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(cls->serf->rpc_port);
  server.sin_addr.s_addr = inet_addr(cls->serf->rpc_ip_address);

  r = connect(cls->serf->sockfd, (const struct sockaddr *)&server, sizeof(server));
  if (r) {
    char log_string[128];
    int e = errno;

    snprintf(log_string, sizeof(log_string), "Failed to connect to socket addr<%s> port<%u>",
        cls->serf->rpc_ip_address,
        cls->serf->rpc_port);

    RWLOG_EVENT(
        cls->serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        log_string,
        e);
    goto close_socket;
  }

  flags = fcntl(cls->serf->sockfd, F_GETFL, 0);
  if (flags == -1) {
    RWLOG_EVENT(
        cls->serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "Failed to get flags on socket",
        errno);
    goto close_socket;
  } else {
    flags = flags|O_NONBLOCK;
    r = fcntl(cls->serf->sockfd, F_SETFL, flags);

    if (r) {
      RWLOG_EVENT(
          cls->serf->rwvcs_instance->rwvx->rwlog,
          RwVmHeartbeatLog_notif_MiscError,
          "fcntl for setting non blocking failed",
          errno);
      goto close_socket;
    }
  }

  setup_cfsource(cls->serf);

  //send the handshake
  status = serf_send_command(
      cls->serf,
      "handshake",
      ++cls->serf->seqno,
      "su",
      "Version",
      1);
  if (status != RW_STATUS_SUCCESS) {
    RWLOG_EVENT(
        cls->serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "Failed to send handshake socket",
        0);
    goto close_socket;
  }
  cls->serf->state = SERF_STATE_HANDSHAKE_SENT;
  cls->serf->handshake_seq = cls->serf->seqno;

  rwsched_tasklet_CFRunLoopTimerRelease(cls->serf->rwsched_tasklet, cls->serf->connect_timer);
  cls->serf->connect_timer = NULL;
  free(cls);

  goto done;

close_socket:
  close(cls->serf->sockfd);
  cls->serf->sockfd = -1;
  cls->attempts++;
  if (cls->attempts >= cls->max_attempts) {
    RWLOG_EVENT(
        cls->serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "Failed to connect to SERF RPC agent",
        0);

    RWTRACE_CRIT(
        cls->serf->rwvcs_instance->rwvx->rwtrace,
        RWTRACE_CATEGORY_RWVCS,
        "Failed to connect to SERF RPC agent after %d attempts, %.2f interval",
        cls->max_attempts,
        cls->interval);

    rwsched_tasklet_CFRunLoopTimerRelease(cls->serf->rwsched_tasklet, cls->serf->connect_timer);
    cls->serf->connect_timer = NULL;
    free(cls);
  }

done:
  return;
}

void serf_rpc_connect(serf_context_ptr_t serf, double interval, int max_attempts)
{
  struct serf_connect_closure * cls;
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };

  cls = (struct serf_connect_closure *)malloc(sizeof(struct serf_connect_closure));
  RW_ASSERT(cls);

  cls->serf = serf;
  cls->interval = interval;
  cls->attempts = 0;
  cls->max_attempts = max_attempts > 0 ? max_attempts : 0;

  cf_context.info = (void *)cls;

  serf->connect_timer = rwsched_tasklet_CFRunLoopTimerCreate(
      serf->rwsched_tasklet,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + interval,
      interval,
      0,
      0,
      &try_rpc_connect,
      &cf_context);

  rwsched_tasklet_CFRunLoopAddTimer(
      serf->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(serf->rwsched_tasklet),
      serf->connect_timer,
      rwsched_instance_CFRunLoopGetMainMode(serf->rwsched));
}

void
serf_instance_free(serf_context_ptr_t serf)
{
  serf_cleanup_instance(serf);
  free(serf);
}

static void
serf_cleanup_instance(
    serf_context_ptr_t serf)
{
  if (serf->sockfd != -1) {
    close(serf->sockfd);
    serf->sockfd = -1;
  }

  if (serf->mpac) {
    msgpack_unpacker_free(serf->mpac);
    serf->mpac = NULL;
  }

  if (serf->connect_timer) {
    rwsched_tasklet_CFRunLoopTimerRelease(serf->rwsched_tasklet, serf->connect_timer);
    serf->connect_timer = NULL;
  }

  if (serf->cfsource) {
    rwsched_tasklet_CFRunLoopRemoveSource(
        serf->rwsched_tasklet,
        rwsched_tasklet_CFRunLoopGetCurrent(serf->rwsched_tasklet),
        serf->cfsource,
        rwsched_instance_CFRunLoopGetMainMode(serf->rwsched));
    rwsched_tasklet_CFSocketReleaseRunLoopSource(serf->rwsched_tasklet, serf->cfsource);
    serf->cfsource = NULL;
  }

  if(serf->cfsocket) {
    rwsched_tasklet_CFSocketInvalidate(serf->rwsched_tasklet, serf->cfsocket);
    rwsched_tasklet_CFSocketRelease(serf->rwsched_tasklet, serf->cfsocket);
    serf->cfsocket = NULL;
  }
}

rw_status_t serf_send_command(serf_context_ptr_t serf, const char * command, uint32_t seq, const char * format, ...)
{
  va_list ap;
  ssize_t sent;
  rw_status_t status;
  msgpack_sbuffer sbuf;
  msgpack_packer packer;

  if (strlen(format) % 2)
    return RW_STATUS_FAILURE;

  msgpack_sbuffer_init(&sbuf);
  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);

  // Pack the header
  msgpack_pack_map(&packer, 2);

  msgpack_pack_str(&packer, 7);
  msgpack_pack_str_body(&packer, "Command", 7);
  msgpack_pack_str(&packer, strlen(command));
  msgpack_pack_str_body(&packer, command, strlen(command));

  msgpack_pack_str(&packer, 3);
  msgpack_pack_str_body(&packer, "Seq", 3);
  msgpack_pack_uint32(&packer, seq);


  msgpack_pack_map(&packer, strlen(format) / 2);

  va_start(ap, format);
  for (int i = 0; i < strlen(format); ++i) {
    switch (format[i]) {
      case 's': {
        char * s = va_arg(ap, char *);

        msgpack_pack_str(&packer, strlen(s));
        msgpack_pack_str_body(&packer, s, strlen(s));
        break;
      }

      case 'u': {
        uint32_t u = va_arg(ap, uint32_t);

        msgpack_pack_uint32(&packer, u);
        break;
      }

      default: {
        va_end(ap);
        status = RW_STATUS_FAILURE;
        goto done;
      }
    }
  }
  va_end(ap);

  sent = send(serf->sockfd, sbuf.data, sbuf.size, 0);
  if (sent < 0) {
    RWLOG_EVENT(
        serf->rwvcs_instance->rwvx->rwlog,
        RwVmHeartbeatLog_notif_MiscError,
        "Failed to send data through socket",
        errno);
    status = RW_STATUS_FAILURE;
  } else {
    status = RW_STATUS_SUCCESS;
  }

done:
  msgpack_sbuffer_destroy(&sbuf);

  return status;
}

struct serf_event_callback * serf_register_event_callback(
    serf_context_ptr_t serf,
    serf_event_callback_t callback,
    void * ctx)
{
  struct serf_event_callback * ec;

  ec = (struct serf_event_callback * )malloc(sizeof(struct serf_event_callback));
  RW_ASSERT(ec);

  ec->callback = callback;
  ec->ctx = ctx;

  SLIST_INSERT_HEAD(&(serf->event_cbs), ec, _list);
  return ec;
}

void serf_free_event_callback(
    serf_context_ptr_t serf,
    struct serf_event_callback * ec)
{
  SLIST_REMOVE(&(serf->event_cbs), ec, serf_event_callback, _list);

  free(ec);
}

