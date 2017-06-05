/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwuagent_nb_req_confd_cdb.cpp
 *
 * Management agent confd northbound handler for config
 */

#include "rwuagent.hpp"

using namespace rw_uagent;
using namespace rw_yang;

static const int RELOAD_MAX_RETRY = 5;
static int reload_count = 0;

/*
   This class needs to be split into 2 classes.  It fails at separation
   of concerns - namely, it has two sockets that have completely
   different purposes.  Split data_fd_ out of here - it does not
   belong.
 */

void rw_management_agent_xml_confd_log_cb(void *user_data,
                                          rw_xml_log_level_e level,
                                          const char *fn,
                                          const char *log_msg)
{
  /*
   * ATTN: These messages need to get back to the transaction that
   * generated them, so that they can be included in the NETCONF error
   * response, if any.
   *
   * ATTN: I think RW.XML needs more context when generating messages -
   * just binding the errors to manage is insufficient - need to
   * actually bind the messages to a particular client/xact.
   */
  auto *inst = static_cast<Instance*>(user_data);

  switch (level) {
    case RW_XML_LOG_LEVEL_DEBUG:
      RW_MA_DOMMGR_LOG (inst, DommgrDebug, fn, log_msg);
      break;
    case RW_XML_LOG_LEVEL_INFO:
      RW_MA_DOMMGR_LOG (inst, DommgrNotice, fn, log_msg);
      break;
    case RW_XML_LOG_LEVEL_ERROR:
      RW_MA_DOMMGR_LOG (inst, DommgrError, fn, log_msg);
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
}


NbReqConfdConfig::NbReqConfdConfig(
  Instance* instance )
: NbReq(
    instance,
    "NbReqConfdConfig",
    RW_MGMTAGT_NB_REQ_TYPE_CONFDCONFIG )
{
  reload_q_ = rwsched_dispatch_queue_create(
                    instance_->rwsched_tasklet(),
                    "reload-conf-q",
                    RWSCHED_DISPATCH_QUEUE_SERIAL);
}

NbReqConfdConfig::~NbReqConfdConfig ()
{
}


rw_status_t NbReqConfdConfig::setup_confd_subscription()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "setup confd subscription");
  rw_status_t ret;

  RW_ASSERT(instance_->confd_addr_);

  if (confd_load_schemas(instance_->confd_addr_, instance_->confd_addr_size_) != CONFD_OK) {
    RW_MA_INST_LOG (instance_, InstanceNotice,
                    "RW.uAgent - load of  Confdb schema failed. Will retry");
    return RW_STATUS_FAILURE;
  }

  RW_MA_INST_LOG(instance_, InstanceDebug, "RW.uAgent - confd load schema done");

  // annotate the YANG model with tailf hash tags
  annotate_yang_model_confd ();

  ret = setup();
  if (ret != RW_STATUS_SUCCESS) {
    RW_MA_INST_LOG (instance_, InstanceNotice, "RW.uAgent - confd_config_ setup failed");
    return RW_STATUS_FAILURE;
  }

  RW_MA_INST_LOG(instance_, InstanceNotice, "RW.uAgent - Moving on to confd reload phase");

  instance_->mgmt_handler()->proceed_to_next_state();

  return RW_STATUS_SUCCESS;
}


void NbReqConfdConfig::annotate_yang_model_confd()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "annotate yang model");

  namespace_map_t ns_map;
  struct confd_nsinfo *listp;

  uint32_t ns_count = confd_get_nslist(&listp);

  RW_ASSERT (ns_count); // for now

  for (uint32_t i = 0; i < ns_count; i++) {
    ns_map[listp[i].uri] = listp[i].hash;
  }

  rw_confd_annotate_ynodes (instance_->yang_model(),
                            ns_map,
                            confd_str2hash,
                            YANGMODEL_ANNOTATION_CONFD_NS,
                            YANGMODEL_ANNOTATION_CONFD_NAME );
}


rw_status_t NbReqConfdConfig::connect(int *fd, cdb_sock_type type)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "connect");

  RW_MA_NBREQ_LOG_FD (this, "Connecting ", *fd);
  if (*fd != RWUAGENT_INVALID_SOCK_ID) {
    return RW_STATUS_SUCCESS;
  }

  RW_ASSERT(instance_->confd_addr_);
  *fd = socket(instance_->confd_addr_->sa_family, SOCK_STREAM, 0);
  RW_ASSERT (*fd >= 0);

  fcntl (*fd, F_SETFD, FD_CLOEXEC);

  if (cdb_connect(*fd, type, instance_->confd_addr_, instance_->confd_addr_size_) < 0) {
    RW_MA_NBREQ_LOG (this, ClientDebug, "Connecting to CDB failed", "");
    close (*fd);
    *fd = RWUAGENT_INVALID_SOCK_ID;
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t NbReqConfdConfig::setup()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "setup");

  rw_status_t ret = RW_STATUS_SUCCESS;
  CFSocketContext cf_context = { 0, this, NULL, NULL, NULL };
  rwsched_instance_ptr_t sched = instance_->rwsched();
  rwsched_tasklet_ptr_t tasklet = instance_->rwsched_tasklet();

  RW_MA_NBREQ_LOG (this, ClientDebug, "Setup", "");

  if (RWUAGENT_INVALID_SOCK_ID == data_fd_) {
    ret = connect (&data_fd_, CDB_DATA_SOCKET);

    if (RW_STATUS_SUCCESS != ret) {
      RW_MA_NBREQ_LOG (this, ClientError, "Confd Data socket failed", "");
      return ret;
    }
  }


  if (RWUAGENT_INVALID_SOCK_ID == sub_fd_) {
    ret = connect (&sub_fd_, CDB_SUBSCRIPTION_SOCKET);

    if (RW_STATUS_SUCCESS != ret) {
      RW_MA_NBREQ_LOG (this, ClientError, "Confd subscription socket failed", "");
      return ret;
    }
  }

  if (cdb_subscribe2 (sub_fd_, CDB_SUB_RUNNING_TWOPHASE, 
                      CDB_SUB_WANT_ABORT_ON_ABORT, 0, &sub_id_, 0, "/") < 0) {
    RW_MA_NBREQ_LOG (this, ClientError, "Confd Subscription failed", "");
    return RW_STATUS_FAILURE;
  }

  RW_MA_NBREQ_LOG (this, ClientDebug, "Confd Subscription succeeded", "");

  if (cdb_subscribe_done (sub_fd_) < 0) {
    RW_MA_NBREQ_LOG (this, ClientError, "Confd done failed", "");
    return RW_STATUS_FAILURE;
  }

  sub_sock_ref_ =  rwsched_tasklet_CFSocketCreateWithNative (tasklet,
                                                     kCFAllocatorSystemDefault,
                                                     sub_fd_,
                                                     kCFSocketReadCallBack,
                                                     &cfcb_confd_sub_event,
                                                     &cf_context );

  RW_CF_TYPE_VALIDATE(sub_sock_ref_, rwsched_CFSocketRef);

  sub_sock_src_ = rwsched_tasklet_CFSocketCreateRunLoopSource(
      tasklet,
      kCFAllocatorSystemDefault,
      sub_sock_ref_,
      0 );

  cf_srcs_[sub_sock_ref_] = sub_sock_src_;

  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tasklet);
  rwsched_tasklet_CFRunLoopAddSource(tasklet, runloop, sub_sock_src_, sched->main_cfrunloop_mode);

  return RW_STATUS_SUCCESS;
}

void NbReqConfdConfig::close_cf_socket(rwsched_CFSocketRef s)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "close cf socket");
  rwsched_instance_ptr_t sched = instance_->rwsched();
  rwsched_tasklet_ptr_t tasklet = instance_->rwsched_tasklet();
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tasklet);

  cf_src_map_t::iterator src = cf_srcs_.find (s);
  RW_ASSERT (src != cf_srcs_.end());

  rwsched_tasklet_CFRunLoopRemoveSource(tasklet,
                                        runloop,
                                        src->second,
                                        sched->main_cfrunloop_mode);
  cf_srcs_.erase(src);

  rwsched_tasklet_CFSocketRelease(tasklet, s);
}

void NbReqConfdConfig::cfcb_confd_sub_event(rwsched_CFSocketRef s,
                                             CFSocketCallBackType callbackType,
                                             CFDataRef address,
                                             const void* data,
                                             void* vconfig)
{

  UNUSED (callbackType);
  UNUSED (address);
  UNUSED (data);

  NbReqConfdConfig *config = static_cast<NbReqConfdConfig *>(vconfig);
  RW_ASSERT(s == config->sub_sock_ref_);

  config->process_confd_subscription();
}

void NbReqConfdConfig::process_confd_subscription()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "confd subscription");
  int confd_result, flags, length, *subscription_points;
  enum cdb_sub_notification type;

  RW_MA_NBREQ_LOG (this, ClientDebug, __FUNCTION__ , "");

  confd_result =
      cdb_read_subscription_socket2(sub_fd_, &type, &flags,
                                    &subscription_points, &length);

  if (confd_result == CONFD_EOF) {
    RW_MA_NBREQ_LOG (this, ClientDebug, __FUNCTION__ , " Reopening subscription socket");
    close_cf_socket(sub_sock_ref_);
    close (sub_fd_);
    sub_fd_ = RWUAGENT_INVALID_SOCK_ID;
    setup();

    return;
  }

  switch (type) {
    case CDB_SUB_PREPARE:
      RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "CDB_SUB_PREPARE");
      RW_MA_NBREQ_LOG (this, ClientDebug, __FUNCTION__ , "Prepare callback from confd");
      prepare (subscription_points, length, flags);
      break;

    case CDB_SUB_COMMIT:
      RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "CDB_SUB_COMMIT");
      RW_MA_NBREQ_LOG (this, ClientDebug, __FUNCTION__ , "Commit callback from confd");
      NbReq::commit_changes();
      cdb_sync_subscription_socket (sub_fd_, CDB_DONE_PRIORITY);
      break;

    case CDB_SUB_ABORT:
      RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "CDB_SUB_ABORT");
      RW_MA_NBREQ_LOG (this, ClientDebug, __FUNCTION__ , "Abort callback from confd");
      cdb_sync_subscription_socket (sub_fd_, CDB_DONE_PRIORITY);
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }
}

void NbReqConfdConfig::async_reload_configuration(NbReqConfdConfig* self)
{
  if (reload_count >= RELOAD_MAX_RETRY) {
    std::string log = "Reload failed after " + std::to_string(reload_count) + " attempts.";
    RW_MA_NBREQ_LOG (self, ClientError, __FUNCTION__, log.c_str());

    // Proceed with confd startup
    self->instance_->mgmt_handler()->proceed_to_next_state();
    return;
  }
  reload_count++;

  if (self->reload_fd_ != RWUAGENT_INVALID_SOCK_ID) {
    close(self->reload_fd_);
    self->reload_fd_ = RWUAGENT_INVALID_SOCK_ID;
  }

  auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5);
  rwsched_dispatch_after_f(self->instance_->rwsched_tasklet(),
                           when,
                           self->reload_q_,
                           self,
                           NbReqConfdConfig::reload_configuration);

  return;
}

void NbReqConfdConfig::reload_configuration(void* ctxt)
{
  auto* self = reinterpret_cast<NbReqConfdConfig*>(ctxt);
  RW_MA_NBREQ_LOG (self, ClientCritInfo, __FUNCTION__, "Starting config reload");

  if (!self->instance_->dts_ready()) {
    RW_MA_NBREQ_LOG (self, ClientDebug, __FUNCTION__, "DTS not yet ready. Will retry");
    async_reload_configuration(self);
    return;
  }

  if (RW_STATUS_SUCCESS != 
            self->connect (&self->reload_fd_, CDB_DATA_SOCKET)) {
      RW_MA_NBREQ_LOG (self, ClientError, "Confd Reload Data socket failed. Retrying.", "");
      async_reload_configuration(self);
      return;
  }

  // Wait till CDB is ready for read
  RWMEMLOG_TIME_SCOPE(self->memlog_buf_, RWMEMLOG_MEM2, "reload config");
  RW_MA_NBREQ_LOG (self, ClientDebug, __FUNCTION__, "Starting config reload");

  if (cdb_wait_start(self->reload_fd_) != CONFD_OK) {
    RW_MA_NBREQ_LOG (self, ClientError, __FUNCTION__, 
        "Error while waiting for CDB to be ready for read");
    async_reload_configuration(self);
    return;
  }

  RW_MA_NBREQ_LOG (self, ClientDebug, __FUNCTION__, "CDB is ready for read op");

  // Blocks till 'prepare()' is completed
  if (cdb_trigger_subscriptions(self->reload_fd_, nullptr, 0) != CONFD_OK)
  {
    RW_MA_NBREQ_LOG (self, ClientError, __FUNCTION__,
        "CDB trigger subscription failed with error. Retrying...");
    async_reload_configuration(self);
    return;
  }

  { // Cleanup
    close (self->reload_fd_);
    self->reload_fd_ = RWUAGENT_INVALID_SOCK_ID;
  }

  RW_MA_NBREQ_LOG (self, ClientCritInfo, __FUNCTION__, "Config reload finished");

  self->instance_->mgmt_handler()->proceed_to_next_state();

  RW_MA_NBREQ_LOG (self, ClientDebug, __FUNCTION__, "CDB configuration reload completed");
  return;
}

void NbReqConfdConfig::prepare(int *subscription_points, int length, int flags)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "prepare");
  confd_tag_value_t *values;
  int nvalues, i;

  RW_ASSERT(!xact_);
  if (!instance_->dts_ready()) {
    cdb_sub_abort_trans (sub_fd_, CONFD_ERRCODE_RESOURCE_DENIED, 0, 0,
                         "DTS has not started yet");
    return;
  }

  // Create a DOM of the changes
  XMLBuilder *builder = new XMLBuilder (instance_->xml_mgr());
  builder->set_log_cb(rw_management_agent_xml_confd_log_cb, instance_);

  // Build a DOM from the modifications
  for (i = 0; i < length; i++) {
    if (cdb_get_modifications(sub_fd_, subscription_points[i], CDB_GET_MODS_INCLUDE_LISTS
                , &values, &nvalues, "/") == CONFD_OK) {
      ConfdTLVTraverser traverser(builder, values, nvalues);
      traverser.set_log_cb(rw_management_agent_xml_confd_log_cb, instance_);
      traverser.traverse();

      RWMEMLOG_BARE (memlog_buf_, RWMEMLOG_MEM6,
          RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, builder->merge_->to_string().c_str()));

      for (int j = 0; j < nvalues; j++) {
        confd_free_value(CONFD_GET_TAG_VALUE(&values[j]));
      }
    }
  }

  if ((flags & CDB_SUB_FLAG_IS_LAST)) {
    xact_ = new SbReqEditConfig(instance_,
                        this,
                        RequestMode::CONFD,
                        builder
                        );
    RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "New edit cfg trx",
        RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64, (intptr_t)xact_));
    xact_->start_xact();
    // Don't care about start status
  } else {
    RW_MA_NBREQ_LOG (this, ClientError, __FUNCTION__ , "Last flag not set ");
  }
}

void NbReqConfdConfig::commit_changes()
{
  RW_ASSERT (xact_);
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "commit changes",
      RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)xact_) );

  rw_status_t rs = rwdts_xact_commit(xact_->dts_xact());
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
}

void NbReqConfdConfig::abort_changes()
{
  RW_ASSERT (xact_);
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "abort changes",
      RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)xact_) );

  rw_status_t rs = rwdts_xact_abort(xact_->dts_xact(), RW_STATUS_FAILURE, 
      "Confd aborted transaction");
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
}

StartStatus NbReqConfdConfig::respond(
  SbReq* sbreq )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  if (xact_) {
    sb_delta_ = xact_->move_delta();
    sb_delete_ks_ = xact_->move_deletes();
    xact_ = nullptr;
  }
  cdb_sync_subscription_socket (sub_fd_, CDB_DONE_PRIORITY);
  return StartStatus::Done;
}

StartStatus NbReqConfdConfig::respond(
  SbReq* sbreq,
  rw_yang::XMLDocument::uptr_t rsp_dom )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond eat dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  return respond( sbreq );
}

StartStatus NbReqConfdConfig::respond(
  SbReq* sbreq,
  NetconfErrorList* nc_errors )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond with error",
            RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
            RWMEMLOG_ARG_PRINTF_INTPTR("nc_errors=0x%" PRIX64,(intptr_t)nc_errors) );

  RW_ASSERT(nc_errors);

  // ATTN: Not a big deal to send a space in case of
  // no error
  std::string err_str(" ");

  // Send the first error in the list as part of abort
  for (auto& elem : nc_errors->get_errors()) {
    if (elem.get_error_path()) {
      err_str += elem.get_error_path();
      err_str += " : ";
    }
    if (elem.get_error_message()) {
      err_str += elem.get_error_message();
    }
    err_str += "\n";
  }

  std::string log = "Confd transaction failed : " + err_str;
  RW_MA_NBREQ_LOG (this, ClientError, __FUNCTION__, log.c_str());

  if (instance_->mgmt_handler()->is_under_reload()) {
    RW_MA_NBREQ_LOG (this, ClientCritInfo, __FUNCTION__, 
        "Marking the failed transaction as success under reload");

    return respond (sbreq);
  }

  cdb_sub_abort_trans (sub_fd_, CONFD_ERRCODE_RESOURCE_DENIED, 0, 0,
                        "%s", err_str.c_str());

  RW_MA_NBREQ_LOG (this, ClientError, __FUNCTION__ ,
                   " Aborting confd transaction");
  xact_ = nullptr;

  return StartStatus::Done;
}

