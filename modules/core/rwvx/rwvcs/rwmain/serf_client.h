
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwvcs_serf.h
 * @author Shaji Radhakrishnan(shaji.radhakrishnan@riftio.com)
 * @date 12/17/2014
 * @brief RW SERF include header file
 */

#ifndef __RWVCS_SERF_H__
#define __RWVCS_SERF_H__

#include <sys/queue.h>

#include <msgpack.h>

__BEGIN_DECLS

#define SERF_RPC_PORT 7373
#define SERF_BIND_PORT 7946
#define SERF_VCS_EVENT_PORT 7891

typedef enum {
  SERF_STATE_CLOSED = 0,
  SERF_STATE_HANDSHAKE_SENT,
  SERF_STATE_STREAM_SENT,
  SERF_STATE_MONITOR_SENT,
  SERF_STATE_ESTABLISHED
} serf_state;

typedef struct  {
  rwlog_severity_t log_level;
  const char *serf_string;
} serf_log_level_str_t;

struct serf_member {
  char * name;
  char address[16];
  char * status;
};

typedef void (*serf_event_callback_t)(void * ctx, const char * event, const struct serf_member *);

struct serf_event_callback {
  SLIST_ENTRY(serf_event_callback) _list;

  serf_event_callback_t callback;
  void * ctx;
};

struct serf_context {
  serf_state state;
  msgpack_unpacker * mpac;

  uint32_t handshake_seq;
  uint32_t stream_seq;
  uint32_t monitor_seq;
  uint32_t last_seq;

  unsigned short rpc_port;
  char rpc_ip_address[256];

  int sockfd;
  uint32_t seqno;

  rwvcs_instance_ptr_t rwvcs_instance;
  rwsched_instance_ptr_t rwsched;
  rwsched_tasklet_ptr_t rwsched_tasklet;
  rwlog_severity_t log_level;

  rwsched_CFRunLoopTimerRef connect_timer;
  rwsched_CFSocketRef cfsocket;
  rwsched_CFRunLoopSourceRef cfsource;

  SLIST_HEAD(_events_head, serf_event_callback) event_cbs;
};

typedef struct serf_context * serf_context_ptr_t;

/*
 * serf_instance_alloc - This function allocates the serf instance ptr memeory
 * @param rpc_ip_address - ip address of the serf agent rpc client
 * @param rpc_port       - tcp port of the serf agent rpc client
 * @param return         - ptr to the allocated memory
 */
serf_context_ptr_t serf_instance_alloc(
    rwvcs_instance_ptr_t rwvcs_instance,
    char *rpc_ip_address,
    unsigned short rpc_port);

/*
 * serf_instance_free - This function frees the serf instance ptr memeory
 *
 * @param serf  - the ptr to the serf instance memory to be freed
 */
void serf_instance_free(serf_context_ptr_t serf);


/*
 * Connect to the rpc client of the started serf agent. This is a tcp socket
 * connection and handshaking.
 *
 * @param instance    -  the ptr to the serf instance memory
 * @param interval    - how ofter to retry connecting
 * @param max_attempt - maximum number of times to try to connect to the SERF
 *                      instance.  Will try forever if < 0.
 */
void serf_rpc_connect(
    serf_context_ptr_t serf,
    double interval,
    int max_attempts);

struct serf_event_callback * serf_register_event_callback(
    serf_context_ptr_t serf,
    serf_event_callback_t callback,
    void * ctx);

void serf_free_event_callback(
    serf_context_ptr_t serf,
    struct serf_event_callback * ec);

__END_DECLS

#endif // __RWVCS_SERF_H__
