
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include "rwtasklet.h"
//#include "rwlogd-c.h"
#include "rwlogd_common.h"
#include "rwlogd.pb-c.h"
#include "rwlogd_rx.h"

static void
rwlogd_file_send_log(RwlogdPeerAPI_Service    *srv,
                     const RwlogdFileSendLogReq *req,
                     void                           *user_handle,
                     RwlogdFileSendLogRsp_Closure                closure,
                     void                           *closure_data);

static void
rwlogd_send_log(RwlogdPeerAPI_Service   *srv,
                const RwlogdSendLogReq  *req,
                void                    *user_handle,
                RwlogdStatus_Closure    closure,
                void                    *closure_data);

static rw_status_t 
rwlogd_register_client(Rwlogd_Service *service,
                          const RwlogdRegisterReq *input,
                          void *usercontext,
                          RwlogdRegisterReq_Closure closure,
                          void *closure_data);

rw_status_t
rwlogd_create_server_endpoint(rwlogd_instance_ptr_t instance)
{
  rw_status_t rwstatus;
  // Instantiate server interface
  char *my_path;

  int r = asprintf(&my_path, 
                   "/R/%s/%d",
                   RWLOGD_PROC,
                   instance->rwtasklet_info->identity.rwtasklet_instance_id);
  RW_ASSERT(r != -1);
  if ( r == -1) {
    return RW_STATUS_FAILURE;
  }


  RW_ASSERT(instance->rwtasklet_info->rwmsg_endpoint);
  if (!instance->rwtasklet_info->rwmsg_endpoint) {
    return RW_STATUS_FAILURE;
  }
  // Create a server channel that tasklet uses to recieve messages from clients
  instance->sc = rwmsg_srvchan_create(instance->rwtasklet_info->rwmsg_endpoint);
  RW_ASSERT(instance->sc);
  if (!instance->sc) {
    return RW_STATUS_FAILURE;
  }

  //Initialize the protobuf service
  RWLOGD__INITSERVER(&instance->rwlogd_srv, rwlogd_);
  // Bind a single service for tasklet namespace
  rwstatus = rwmsg_srvchan_add_service(instance->sc,
                                       my_path,
                                       (ProtobufCService *)&instance->rwlogd_srv,
                                       instance);
  RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);
  if (rwstatus != RW_STATUS_SUCCESS) {
    return rwstatus;
  }

  RWLOGD_PEER_API__INITSERVER(&instance->rwlogd_peerapi_srv,rwlogd_);
  rwstatus = rwmsg_srvchan_add_service(instance->sc,
                                       my_path,
                                       (ProtobufCService *)&instance->rwlogd_peerapi_srv,
                                       instance);
  RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);
  if (rwstatus != RW_STATUS_SUCCESS) {
    return rwstatus;
  }


  // Bind this srvchan to rwsched's taskletized cfrunloop
  // This will result in events executing in the cfrunloop context
  // rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.rwsched);
  rwstatus = rwmsg_srvchan_bind_rwsched_cfrunloop(instance->sc,
                                                    instance->rwtasklet_info->rwsched_tasklet_info);
  RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);
  if (rwstatus != RW_STATUS_SUCCESS) {
    return rwstatus;
  }

  free(my_path);
  return rwstatus;
}

static rw_status_t 
rwlogd_register_client(Rwlogd_Service *service,
                          const RwlogdRegisterReq *input,
                          void *usercontext,
                          RwlogdRegisterReq_Closure closure,
                          void *closure_data)
{
   // Create the shared memory here .. 
   //clo(NULL, closure_data);
   //printf ("rwlogd_register_client Received request from client key: %s", input->my_key);

  return RW_STATUS_SUCCESS;
}

static void
rwlogd_file_send_log(RwlogdPeerAPI_Service    *srv,
                                              const RwlogdFileSendLogReq *req,
                                              void                           *user_handle,
                                              RwlogdFileSendLogRsp_Closure    closure,
                                              void                           *closure_data)
{
  rwlogd_instance_ptr_t       instance;
  RwlogdFileSendLogRsp                     rsp;

  // Validate input parameters
  RW_ASSERT(srv);
  if (!srv) { return; }
  RW_ASSERT(req);
  if (!req) { return; }
  RW_ASSERT(user_handle);
  if (!user_handle) { return; }
  RW_ASSERT(closure);
  if (!closure) { return; }
  RW_ASSERT(closure_data);
  if (!closure_data) { return; }

  instance = (rwlogd_instance_ptr_t) user_handle;
  //RW_CF_TYPE_VALIDATE(instance, rwlogd_instance_ptr_t);

  rwlogd_handle_file_log(instance->rwlogd_info, req->my_key,req->sink_name);

  // Initialize response structure
  rwlogd_file_send_log_rsp__init(&rsp);
  closure(&rsp, closure_data);

  return;
}
  
static void
rwlogd_send_log(RwlogdPeerAPI_Service   *srv,
                const RwlogdSendLogReq  *req,
                void                    *user_handle,
                RwlogdStatus_Closure    closure,
                void                    *closure_data)
{
  rwlogd_instance_ptr_t       instance;
  RwlogdStatus                     rsp;
  size_t log_size = 0;
  size_t offset = 0;

  // Validate input parameters
  RW_ASSERT(srv);
  if (!srv) { return; }
  RW_ASSERT(req);
  if (!req) { return; }
  RW_ASSERT(user_handle);
  if (!user_handle) { return; }
  RW_ASSERT(closure);
  if (!closure) { return; }
  RW_ASSERT(closure_data);
  if (!closure_data) { return; }

  instance = (rwlogd_instance_ptr_t) user_handle;
  //RW_CF_TYPE_VALIDATE(instance, rwlogd_instance_ptr_t);
  
  RWLOG_DEBUG_PRINT("Received Log in instance: %d of length %lu\n", instance->rwtasklet_info->identity.rwtasklet_instance_id,req->log_msg.len);
  instance->rwlogd_info->stats.peer_recv_requests++;

  while(offset < req->log_msg.len) 
  {
    rwlog_hdr_t *hdr = (rwlog_hdr_t *)(req->log_msg.data + offset);

    if(hdr->magic != RWLOG_MAGIC || ((offset + hdr->size_of_proto+sizeof(rwlog_hdr_t)) > req->log_msg.len) ||
       (hdr->log_category > instance->rwlogd_info->num_categories || hdr->log_severity > RW_LOG_LOG_SEVERITY_DEBUG)) {
      RWLOG_DEBUG_PRINT("read %dl Magic%d category:%u Severity:%u; rotating\n",
                        (int)req->log_msg.len, hdr->magic,hdr->log_category,hdr->log_severity);
      instance->rwlogd_info->stats.invalid_log_from_peer++;

      rwlogd_status__init(&rsp);
      rsp.status = RWLOGD_STATUS__MSGSTATUS__FAILURE;
      closure(&rsp, closure_data);

      return;
    }

    log_size = hdr->size_of_proto+sizeof(rwlog_hdr_t);
    offset += log_size;
    rwlogd_handle_log(instance,(uint8_t *)hdr,log_size,TRUE);
    instance->rwlogd_info->stats.logs_received_from_peer++;
  }

  // Initialize response structure
  rwlogd_status__init(&rsp);
  rsp.status = RWLOGD_STATUS__MSGSTATUS__SUCCESS;
  closure(&rsp, closure_data);

  return;
}

