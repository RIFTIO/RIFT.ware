/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwuagent_confd_daemon.cpp
 *
 * Management agent confd daemon management.
 */

#include "rwuagent.hpp"
#include "rwuagent_confd.hpp"
#include "rwuagent_log_file_manager.hpp"
#include "rw-mgmtagt-confd.pb-c.h"

using namespace rw_uagent;
using namespace rw_yang;


/*
  ATTN: Shouldn't all these static functions be class statics?
 */

static int execute_confd_action(struct confd_user_info *ctxt,
                                struct xml_tag *name, confd_hkeypath_t *kp,
                                confd_tag_value_t *params, int nparams)
{
  NbReqConfdRpcExec *rpc = static_cast <NbReqConfdRpcExec *> (ctxt->u_opaque);
  return rpc->execute (name, kp, params, nparams);
}

static int get_confd_elem(struct confd_trans_ctx *ctxt,
                          confd_hkeypath_t *keypath)
{
  NbReqConfdDataProvider *dp =
      static_cast <NbReqConfdDataProvider *> (ctxt->t_opaque);

  return dp->confd_get_elem(ctxt,keypath);
}

static int get_confd_case (struct confd_trans_ctx *ctxt,
                           confd_hkeypath_t *keypath,
                           confd_value_t *choice)
{
  UNUSED (choice);
  UNUSED (keypath);
  UNUSED (choice);

  NbReqConfdDataProvider *dp =
      static_cast <NbReqConfdDataProvider *> (ctxt->t_opaque);

  return dp->confd_get_case(ctxt,keypath,choice);
}

static int get_confd_next(struct confd_trans_ctx *ctxt,
                          confd_hkeypath_t *keypath,
                          long next)
{
  NbReqConfdDataProvider *dp =
      static_cast <NbReqConfdDataProvider *> (ctxt->t_opaque);
  return dp->confd_get_next(ctxt, keypath, next);
}

static int get_confd_object(struct confd_trans_ctx *ctxt,
                            confd_hkeypath_t *keypath)
{
  NbReqConfdDataProvider *dp =
      static_cast <NbReqConfdDataProvider *> (ctxt->t_opaque);
  return dp->confd_get_object(ctxt, keypath);
}

static int get_confd_next_object(struct confd_trans_ctx *ctxt,
                                 confd_hkeypath_t *keypath,
                                 long next)
{
  NbReqConfdDataProvider *dp =
      static_cast <NbReqConfdDataProvider *> (ctxt->t_opaque);
  return dp->confd_get_next_object(ctxt, keypath,next);
}

static int get_confd_exists(struct confd_trans_ctx *ctxt,
                          confd_hkeypath_t *keypath)
{
  NbReqConfdDataProvider *dp =
      static_cast <NbReqConfdDataProvider *> (ctxt->t_opaque);

  return dp->confd_get_exists(ctxt,keypath);
}

static int init_confd_transaction(struct confd_trans_ctx *ctxt)
{
  ConfdDaemon *daemon = static_cast <ConfdDaemon *> (ctxt->dx->d_opaque);
  return daemon->init_confd_trans(ctxt);
}

static int finish_confd_transaction(struct confd_trans_ctx *ctxt)
{
  ConfdDaemon *daemon = static_cast <ConfdDaemon *> (ctxt->dx->d_opaque);
  return daemon->finish_confd_trans (ctxt);
}

static int init_confd_action(struct confd_user_info *ctxt)
{
  ConfdDaemon *daemon = static_cast <ConfdDaemon *> (ctxt->actx.dx->d_opaque);
  return daemon->init_confd_rpc(ctxt);
}

static void
rw_uagent_confd_connection_retry_callback(void *user_context)
{
  auto* self = static_cast <ConfdDaemon*> (user_context);
  self->try_confd_connection();
  return;
}


ConfdDaemon::ConfdDaemon(Instance* instance)
  : instance_ (instance)
  , confd_log_(instance_->log_file_manager())
  , confd_config_(new NbReqConfdConfig(instance_))
  , memlog_buf_(
      instance->get_memlog_inst(),
      "ConfdDaemon",
      reinterpret_cast<intptr_t>(this))
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "created");

  confd_log_->add_confd_log_files();

  size_t cores = RWUAGENT_DAEMON_DEFAULT_POOL_SIZE;
  dp_q_pool_.reserve(cores);

  for (size_t i = 0; i < cores; i++) {
    std::string queue_name = "dp-q-" + std::to_string(i);
    dp_q_pool_.push_back(
        rwsched_dispatch_queue_create(
                     instance_->rwsched_tasklet(),
                     queue_name.c_str(),
                     RWSCHED_DISPATCH_QUEUE_SERIAL)
        );
  }

  upgrade_ctxt_.serial_upgrade_q_ = rwsched_dispatch_queue_create(
      instance_->rwsched_tasklet(),
      "upgrade-queue",
      RWSCHED_DISPATCH_QUEUE_SERIAL);
}

ConfdDaemon::~ConfdDaemon()
{
  if (daemon_ctxt_) {
    confd_release_daemon(daemon_ctxt_);
  }
  daemon_ctxt_ = nullptr;
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "destroyed");

  for (auto& q : dp_q_pool_){
    RW_FREE_TYPE(q, rwsched_dispatch_queue_t);
  }
}


void ConfdDaemon::try_confd_connection()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "try confd connection",
           RWMEMLOG_ARG_PRINTF_INTPTR("try=%" PRIdPTR, (intptr_t)confd_connection_attempts_));
  std::string log_str;

  if (setup_confd_connection() == RW_STATUS_SUCCESS) {
    RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "setup conn failed");
    log_str = "RW.uagent - Connection succeded at try ";
    log_str += std::to_string(confd_connection_attempts_);
    RW_MA_INST_LOG(instance_, InstanceCritInfo, log_str.c_str());
    return;
  }

  confd_connection_attempts_++;

  log_str = "RW.uagent - Connection attempt at try ";
  log_str += std::to_string(confd_connection_attempts_);
  RW_MA_INST_LOG (instance_, InstanceNotice, log_str.c_str());

  rwsched_dispatch_after_f(
      instance_->rwsched_tasklet(),
      dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * RWUAGENT_RETRY_CONFD_CONNECTION_TIMEOUT_SEC),
      rwsched_dispatch_get_main_queue(instance_->rwsched()),
      this,
      rw_uagent_confd_connection_retry_callback);
}

rw_status_t ConfdDaemon::setup_confd_connection()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "setup confd connection");

  if (!instance_->mgmt_handler()->is_instance_ready()) {
    RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "instance not ready");
    return RW_STATUS_FAILURE;
  }

  if (instance_->mgmt_handler()->system_init() != RW_STATUS_SUCCESS) {
    RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "connection failed");
    RW_MA_INST_LOG(instance_, InstanceNotice,
        "RW.uAgent - startup_handler - maapi connection failed");
    return RW_STATUS_FAILURE;
  }

  rw_status_t ret = setup();

  if (ret != RW_STATUS_SUCCESS) {
    RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "daemon setup failed");
    RW_MA_INST_LOG(instance_, InstanceNotice, "RW.uAgent - confd_daemon_- setup failed");
    return RW_STATUS_FAILURE;
  }

  RW_MA_INST_LOG(instance_, InstanceNotice, "RW.uAgent - Moving on to subscription phase");
  instance_->mgmt_handler()->proceed_to_next_state();

  return RW_STATUS_SUCCESS;
}

rw_status_t ConfdDaemon::setup()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "setup");

  rwsched_instance_ptr_t sched = instance_->rwsched();
  rwsched_tasklet_ptr_t tasklet = instance_->rwsched_tasklet();

  uint32_t res = rw_yang::ConfdUpgradeMgr().get_max_version_linked();
  upgrade_ctxt_.schema_version_ = res;

  RW_MA_INST_LOG(instance_, InstanceInfo, "RW.uAgent - Schema version updated");

  // release any previous deamons
  if (nullptr != daemon_ctxt_){
    confd_release_daemon(daemon_ctxt_);
    daemon_ctxt_ = nullptr;
  }

  RW_ASSERT(instance_->confd_addr_);

  control_fd_ = socket(instance_->confd_addr_->sa_family, SOCK_STREAM, 0);
  RW_ASSERT (control_fd_ >= 0);

  RW_MA_INST_LOG(instance_, InstanceInfo, "Start confd daemon setup");
  daemon_ctxt_ = confd_init_daemon("rwUagent");

  if (nullptr == daemon_ctxt_ ) {
    RW_MA_INST_LOG (instance_, InstanceError, "Initialization of Confdb daemon failed");
    RW_ASSERT_NOT_REACHED();
  }

  daemon_ctxt_->d_opaque = this;

  fcntl (control_fd_, F_SETFD, FD_CLOEXEC);

  if (confd_connect(daemon_ctxt_, control_fd_, CONTROL_SOCKET,
                    instance_->confd_addr_, instance_->confd_addr_size_) < 0) {
    RW_MA_INST_LOG (instance_, InstanceError,
        "RW.uAgent -  Connect on control socket  failed. Exiting");
    RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "failed connect");
    close (control_fd_);
    return RW_STATUS_FAILURE;
  }

  RW_MA_INST_LOG(instance_, InstanceCritInfo, "RW.uAgent - Successfully connected with confd.");

  struct confd_trans_cbs trans;

  memset (&trans, 0, sizeof (trans));
  trans.init = init_confd_transaction;
  trans.finish = finish_confd_transaction;

  if (CONFD_ERR == confd_register_trans_cb(daemon_ctxt_, &trans)) {
    RW_MA_INST_LOG(instance_, InstanceError, "Registration of confd transaction failed. Exiting");
    RW_ASSERT_NOT_REACHED();
  }

  RW_MA_INST_LOG(instance_, InstanceDebug, "Registration of confd transaction successfull.");

  struct confd_data_cbs data;

  memset(&data, 0, sizeof (struct confd_data_cbs));
  data.get_elem = get_confd_elem;
  data.get_next = get_confd_next;
  data.get_next_object = get_confd_next_object;
  data.get_case = get_confd_case;
  data.exists_optional = get_confd_exists;

  data.get_object = get_confd_object;
  strcpy(data.callpoint, "rw_callpoint");

  if (confd_register_data_cb(daemon_ctxt_, &data) == CONFD_ERR) {
    RW_MA_INST_LOG(instance_, InstanceError, "Registration of confd rw_callpoint failed. Exiting");
    RW_ASSERT_NOT_REACHED();
  }

  RW_MA_INST_LOG(instance_, InstanceDebug, "Registration of confd callpoints successfull.");

  struct confd_action_cbs action;
  memset (&action, 0, sizeof (action));
  strcpy (action.actionpoint, "rw_actionpoint");
  action.init = init_confd_action;
  action.action = execute_confd_action;

  if (confd_register_action_cbs (daemon_ctxt_, &action) == CONFD_ERR) {
    RW_MA_INST_LOG(instance_, InstanceError, " Registration of confd actions failed. Exiting");
    RW_ASSERT_NOT_REACHED();
  }

  RW_MA_INST_LOG(instance_, InstanceDebug, "Registration of confd actionpoints successfull.");
  rw_status_t rw_status = setup_confd_worker_pool();

  RW_ASSERT( RW_STATUS_SUCCESS == rw_status);

  // Register for the known Notification streams
  setup_notifications();

  if (confd_register_done(daemon_ctxt_) != CONFD_OK) {
    RW_MA_INST_LOG(instance_, InstanceError, "Registration done failed. Exiting");
    RW_ASSERT_NOT_REACHED();
  }
  RW_MA_INST_LOG(instance_, InstanceDebug, "Registration of confd successfull.");

  control_sock_src_ = rwsched_dispatch_source_create(tasklet,
                                                     RWSCHED_DISPATCH_SOURCE_TYPE_READ,
                                                     control_fd_,
                                                     0,
                                                     rwsched_dispatch_get_main_queue(sched));
  RW_ASSERT(control_sock_src_);

  auto *dps_ctx = new DispatchSrcContext();
  dps_ctx->confd_    = this;
  dps_ctx->confd_fd_ = control_fd_;
  dps_ctx->dp_src_   = control_sock_src_;

  rwsched_dispatch_set_context(tasklet, control_sock_src_, dps_ctx);
  rwsched_dispatch_source_set_event_handler_f(tasklet,
                                              control_sock_src_,
                                              cfcb_confd_control_event);
  rwsched_dispatch_resume(tasklet, control_sock_src_);

  RW_MA_INST_LOG(instance_, InstanceInfo, "Finish confd daemon setup");

  return RW_STATUS_SUCCESS;
}

rw_status_t ConfdDaemon::setup_notifications()
{
  struct confd_notification_stream_cbs ncb;
  int worker_fd = assign_confd_worker();

  for (const auto& stream_info: instance_->netconf_streams_) {
    confd_notification_ctx* notify_ctxt = nullptr;
    // Register the notification streams
    memset(&ncb, 0, sizeof(ncb));
    ncb.fd = worker_fd;

    strcpy(ncb.streamname, stream_info.name.c_str());

    if (confd_register_notification_stream(daemon_ctxt_, &ncb, &notify_ctxt) 
        != CONFD_OK) {
      RW_MA_INST_LOG(instance_, InstanceError, 
        "Registration of notifications failed. Exiting");
      RW_ASSERT_NOT_REACHED();
    }

    notification_ctxt_map_[stream_info.name] = notify_ctxt;

    RW_MA_INST_LOG(instance_, InstanceInfo, 
      ("Registered for notification stream" + stream_info.name).c_str());
  }

  return RW_STATUS_SUCCESS;
}

rwdts_member_rsp_code_t
ConfdDaemon::handle_notification(
    const ProtobufCMessage * msg)
{
  RW_ASSERT(msg);

  rw_yang_netconf_op_status_t ncs = RW_YANG_NETCONF_OP_STATUS_FAILED;
  rw_status_t status = RW_STATUS_SUCCESS;

  auto pbc_dom = instance_->xml_mgr()->create_document_from_pbcm(
                           const_cast<ProtobufCMessage*>(msg), ncs);

  struct xml_tag tag = {0};
  XMLNode *node = pbc_dom->get_root_node();

  if (!node) {
    return RWDTS_ACTION_NOT_OK;
  }
  std::string lstr;
  RW_MA_INST_LOG(instance_, InstanceDebug,
      (lstr="Notification : " + node->to_string()).c_str());

  status = find_confd_hash_values(node->get_yang_node(), &tag);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  std::string node_name = node->get_local_name();
  std::string node_ns   = node->get_name_space();

  auto *cs_node = confd_cs_node_cd(nullptr, "/%s", node_name.c_str());
  RW_ASSERT(cs_node);

  confd_notification_ctx* notify_ctx = get_notification_context(node_name, node_ns);
  RW_ASSERT(notify_ctx);

  ConfdTLVBuilder builder(cs_node, true);
  XMLTraverser traverser(&builder, node);
  traverser.traverse();

  if (builder.length() == 0) {
    RW_MA_INST_LOG(instance_, InstanceDebug, "No data found for notification");
    return RWDTS_ACTION_NOT_OK;
  }

  return send_notification_to_confd(notify_ctx, builder, tag);
}

confd_notification_ctx* ConfdDaemon::get_notification_context(
                            const std::string& node_name,
                            const std::string& node_ns)
{
  std::string stream = instance_->lookup_notification_stream(node_name, node_ns);
  if (stream.empty()) {
    // Default stream
    stream.assign("uagent_notification");
  }

  auto it = notification_ctxt_map_.find(stream); 
  if (it != notification_ctxt_map_.end()) {
    return it->second;
  }

  return nullptr;
}


rwdts_member_rsp_code_t
ConfdDaemon::send_notification_to_confd(
              confd_notification_ctx* notify_ctxt,
              ConfdTLVBuilder& builder, 
              struct xml_tag xtag)
{
  struct confd_datetime now;
  rwdts_member_rsp_code_t rcode = RWDTS_ACTION_NOT_OK;

  getdatetime(&now);

  // ATTN: For sending notification in Confd TLV format
  // using ConfdTLVBuilder is not enough. This is because
  // ConfdTLVBuilder starts building the TLV from roots child
  // element, not from the root. But, since that is what was required
  // for DP's and RPC, allocating 2 additional 'confd_tag_value_t'
  // to account for the Roots XMLBEGIN and XMLEND tag.

  auto nvals = builder.length() + 2;
  auto size = sizeof(confd_tag_value_t) * nvals;
  confd_tag_value_t* vals = (confd_tag_value_t *) RW_MALLOC(size);

  // Offset by 1 is done since index 0 will hold
  // XMLBEGIN tag
  builder.copy_and_destroy_tag_array(vals + 1);

  // Set the XML begin and end tags at beginning and end
  CONFD_SET_TAG_XMLBEGIN(&vals[0], xtag.tag, xtag.ns);
  CONFD_SET_TAG_XMLEND(&vals[nvals - 1], xtag.tag, xtag.ns);

  auto ret = confd_notification_send(
              notify_ctxt,
              &now,
              vals,
              nvals);

  if (ret != CONFD_OK) {
    std::string log_str = "Notification could not be forwarded: ";
    log_str += confd_lasterr();
    RW_MA_INST_LOG(instance_, InstanceError, log_str.c_str());
    return rcode;
  }

  RW_MA_INST_LOG(instance_, InstanceDebug, "Notification sent successfully");

  return RWDTS_ACTION_OK;
}

rw_yang_netconf_op_status_t ConfdDaemon::get_confd_daemon(XMLNode* node)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "get agent confd stats" );
  RW_ASSERT(node);

  RWPB_M_MSG_DECL_INIT(RwMgmtagtConfd_Confd, confd);
  RWPB_T_MSG(RwMgmtagtConfd_Client) *client = (RWPB_T_MSG(RwMgmtagtConfd_Client) *)
      protobuf_c_message_create(nullptr, RWPB_G_MSG_PBCMD(RwMgmtagtConfd_Client));

  confd.n_client = 1;
  confd.client = &client;

  client->n_cached_dom = dom_stats_.size();
  RWPB_T_MSG(RwMgmtagtConfd_CachedDom) **doms = (RWPB_T_MSG(RwMgmtagtConfd_CachedDom) **)
      //ATTN this has to be based on some alloc on the protobuf allocator
      RW_MALLOC(sizeof(RWPB_T_MSG(RwMgmtagtConfd_CachedDom) *)  * client->n_cached_dom);

  client->cached_dom = doms;
  client->identifier = 0;
  client->has_identifier = 1;

  int i = 0; // fake index
  for (auto& dom : dom_stats_) {
    doms[i] = dom->get_pbcm();
    doms[i]->index = i;
    doms[i]->has_index = 1;
    i++;
  }

  rw_yang_netconf_op_status_t ncrs =
      node->merge ((ProtobufCMessage *)&confd);

  if (ncrs != RW_YANG_NETCONF_OP_STATUS_OK) {
    RW_MA_INST_LOG (instance_, InstanceError, "Failed in merge op");
    return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
  }

  RW_FREE ( doms);
  client->n_cached_dom = 0;
  client->cached_dom = nullptr;

  for (auto dp : dp_clients_) {
    client->identifier = (uint64_t) dp;
    client->n_cached_dom = dp->dom_stats_.size();
    doms = (RWPB_T_MSG(RwMgmtagtConfd_CachedDom) **)
        //ATTN this has to be based on some alloc on the protobuf allocator
        RW_MALLOC (sizeof(RWPB_T_MSG(RwMgmtagtConfd_CachedDom)*)  * client->n_cached_dom);

    client->cached_dom = doms;
    int i = 0; // fake index

    for (auto& dom : dp->dom_stats_) {
      doms[i] = dom->get_pbcm();
      doms[i]->index = i;
      doms[i]->has_index = 1;
      i++;
    }

    rw_yang_netconf_op_status_t ncrs =
        node->merge ((ProtobufCMessage *)&confd);

    if (ncrs != RW_YANG_NETCONF_OP_STATUS_OK) {
      RW_MA_INST_LOG (instance_, InstanceError, "Failed in merge op");
      return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
    }

    RW_FREE ( doms);
    client->n_cached_dom = 0;
    client->cached_dom = nullptr;
  }

  protobuf_c_message_free_unpacked (nullptr, (ProtobufCMessage*)client);
  return RW_YANG_NETCONF_OP_STATUS_OK;
}


rw_yang_netconf_op_status_t ConfdDaemon::get_dom_refresh_period(XMLNode* node)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "get agent refresh" );
  RW_ASSERT(node);

  RWPB_M_MSG_DECL_INIT(RwMgmtagtConfd_Confd, confd);
  RWPB_M_MSG_DECL_INIT(RwMgmtagtConfd_Confd_DomRefreshPeriod, refresh_period);

  // ATTN: confd.has_total_dom_lifetime = true;
  confd.dom_refresh_period = &refresh_period;

  refresh_period.has_cli_dom_refresh_period = true;
  refresh_period.cli_dom_refresh_period = instance_->cli_dom_refresh_period_msec_;

  refresh_period.has_nc_rest_dom_refresh_period = true;
  refresh_period.nc_rest_dom_refresh_period = instance_->nc_rest_refresh_period_msec_;

  rw_yang_netconf_op_status_t ncs = node->merge( &confd.base );
  return ncs;
}


rw_status_t ConfdDaemon::setup_confd_worker_pool()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "create worker pool");

  RW_ASSERT(instance_->confd_addr_);
  rwsched_tasklet_ptr_t tasklet = instance_->rwsched_tasklet();

  worker_fd_vec_.reserve(RW_MAX_CONFD_WORKERS);

  while (worker_fd_vec_.size() < RW_MAX_CONFD_WORKERS) {
    RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "create one worker");

    int worker_fd = socket(instance_->confd_addr_->sa_family, SOCK_STREAM, 0);
    RW_ASSERT (worker_fd >= 0);

    fcntl(worker_fd, F_SETFD, FD_CLOEXEC);

    if (confd_connect(daemon_ctxt_, worker_fd, WORKER_SOCKET,
                      instance_->confd_addr_, instance_->confd_addr_size_) < 0) {
      RW_MA_INST_LOG (instance_, InstanceError, "Connect on worker socket  failed. Exiting");
      close (worker_fd);
      return RW_STATUS_FAILURE;
    }

    rwsched_dispatch_source_t ds_workerfd;
    ds_workerfd = rwsched_dispatch_source_create(tasklet,
                                                 RWSCHED_DISPATCH_SOURCE_TYPE_READ,
                                                 worker_fd,
                                                 0,
                                                 get_dp_q(worker_fd));
    RW_ASSERT(ds_workerfd);

    auto *dps_ctx = new DispatchSrcContext();
    dps_ctx->confd_    = this;
    dps_ctx->confd_fd_ = worker_fd;
    dps_ctx->dp_src_   = ds_workerfd;

    rwsched_dispatch_set_context(tasklet, ds_workerfd, dps_ctx);
    rwsched_dispatch_source_set_event_handler_f(
        tasklet, ds_workerfd, cfcb_confd_worker_event);
    rwsched_dispatch_resume(tasklet, ds_workerfd);

    worker_fd_vec_.push_back(worker_fd);
  }

  return RW_STATUS_SUCCESS;
}

inline void
ConfdDaemon::async_execute_on_main_thread(dispatch_function_t task,
                                          void* ud)
{
  const auto& tasklet = instance_->rwsched_tasklet();
  const auto& sched = instance_->rwsched();

  rwsched_dispatch_async_f(tasklet,
                           rwsched_dispatch_get_main_queue(sched),
                           ud,
                           task);
  return;
}

int ConfdDaemon::assign_confd_worker()
{
  RW_ASSERT(RW_MAX_CONFD_WORKERS > last_alloced_index_);
  int worker_fd = worker_fd_vec_[last_alloced_index_];
  last_alloced_index_ = (last_alloced_index_ + 1) % RW_MAX_CONFD_WORKERS;
  RW_ASSERT(worker_fd);
  return worker_fd;
}

int ConfdDaemon::init_confd_trans(struct confd_trans_ctx *ctxt)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "New transaction with confd",
    RWMEMLOG_ARG_PRINTF_INTPTR("confdctx=%" PRIu64,(intptr_t)ctxt));

  RW_MA_INST_LOG (instance_, InstanceDebug, "New transaction with confd");

  int worker_fd = assign_confd_worker ();
  NbReqConfdDataProvider *dp = new NbReqConfdDataProvider (this, ctxt,
                                                     get_dp_q(worker_fd),
                                                     instance_->cli_dom_refresh_period_msec_,
                                                     instance_->nc_rest_refresh_period_msec_);
  ctxt->t_opaque = dp;
  confd_trans_set_fd (ctxt, worker_fd);

  if (!strcmp(ctxt->uinfo->context, "system")) {
    async_execute_on_main_thread(
             [](void* ctxt)-> void
             {
              auto* dp = static_cast<NbReqConfdDataProvider*>(ctxt);
              auto* daemon = dp->daemon_;
              daemon->dp_clients_.push_back(dp);
             },
        dp
    );
  }

  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "Assigned worker",
    RWMEMLOG_ARG_PRINTF_INTPTR("confdctx=%" PRIu64,(intptr_t)ctxt),
    RWMEMLOG_ARG_PRINTF_INTPTR("%" PRId64,worker_fd));
  return CONFD_OK;
}

int ConfdDaemon::finish_confd_trans(struct confd_trans_ctx *ctxt)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "Finish confd transaction",
    RWMEMLOG_ARG_PRINTF_INTPTR("confdctx=%" PRIu64,(intptr_t)ctxt));
  RW_MA_INST_LOG (instance_, InstanceDebug, "Finish confd transaction");

  auto* dp = static_cast<NbReqConfdDataProvider *>(ctxt->t_opaque);
  RW_ASSERT (dp);

  async_execute_on_main_thread(
    [] (void* ctxt) -> void
    {
      auto* dp = static_cast<NbReqConfdDataProvider*>(ctxt);
      auto* daemon = dp->daemon_;
      std::unique_ptr<NbReqConfdDataProvider> dpu(dp);

      daemon->dom_stats_.splice(daemon->dom_stats_.begin(), dp->dom_stats_);

      if (daemon->dom_stats_.size() > RWUAGENT_MAX_CONFD_STATS_DAEMON) {
        auto remove_begin = std::next(daemon->dom_stats_.begin(),
                                      RWUAGENT_MAX_CONFD_STATS_DAEMON - 1);
        auto remove_end = daemon->dom_stats_.end();
        daemon->dom_stats_.erase(remove_begin, --remove_end);
      }

      RW_ASSERT(daemon->dom_stats_.size() <= RWUAGENT_MAX_CONFD_STATS_DAEMON);

      daemon->dp_clients_.remove(dpu.get());
    },
    dp
  );

  return 0;
}

int ConfdDaemon::init_confd_rpc(struct confd_user_info *ctxt)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "Start confd RPC",
    RWMEMLOG_ARG_PRINTF_INTPTR("confdctx=%" PRIu64,(intptr_t)ctxt));

  NbReqConfdRpcExec *rpc = new NbReqConfdRpcExec(this, ctxt);
  ctxt->u_opaque = rpc;

  int fd = assign_confd_worker();
  confd_action_set_fd (ctxt, fd);

  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "Assigned worker",
    RWMEMLOG_ARG_PRINTF_INTPTR("confdctx=%" PRIu64,(intptr_t)ctxt),
    RWMEMLOG_ARG_PRINTF_INTPTR("%" PRId64,fd));

  RW_MA_NBREQ_LOG_FD (rpc, "New RPC client created with fd", fd);
  return CONFD_OK;
}

void ConfdDaemon::cfcb_confd_worker_event(void* ctx)
{
  auto *dp_ctx = static_cast<DispatchSrcContext *>(ctx);
  RW_ASSERT (dp_ctx);
  dp_ctx->confd_->process_confd_data_req(dp_ctx);
}

void ConfdDaemon::cfcb_confd_control_event(void* ctx)
{
  auto *dp_ctx = static_cast<DispatchSrcContext *>(ctx);
  RW_ASSERT (dp_ctx);
  dp_ctx->confd_->process_confd_data_req(dp_ctx);
}

void ConfdDaemon::process_confd_data_req(DispatchSrcContext *sctxt)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM5, "confd data req");

  RWMEMLOG_TIME_CODE(
    (auto confd_status = confd_fd_ready (daemon_ctxt_, sctxt->confd_fd_);),
    memlog_buf_, RWMEMLOG_LOPRI, "confd_fd_ready" );

  if (confd_status != CONFD_EOF) return;

  rwsched_dispatch_source_cancel(instance_->rwsched_tasklet(),
                                 sctxt->dp_src_);

  if (sctxt->confd_fd_ == control_fd_) {
    async_execute_on_main_thread(
        [](void* ctxt) -> void
        {
          auto* dp_ctx = static_cast<DispatchSrcContext*>(ctxt);
          close (dp_ctx->confd_->control_fd_);
          dp_ctx->confd_->setup();
        },
        sctxt
    );
  } else {
    async_execute_on_main_thread(
        [](void* ctxt) -> void
        {
          auto* dp_ctx = static_cast<DispatchSrcContext*>(ctxt);
          auto *self = dp_ctx->confd_;
          int del_sock = dp_ctx->confd_fd_;
          // remove this fd from the worker pool
          // ATTN: there should be a way to avoid erase on vector
          auto it = std::find(self->worker_fd_vec_.begin(),
                              self->worker_fd_vec_.end(), del_sock);
          if (it != self->worker_fd_vec_.end()) {
            self->worker_fd_vec_.erase(it);
          }

          close (del_sock);
          self->setup_confd_worker_pool();
        },
        sctxt
    );
  }

  return;
}

void ConfdDaemon::clear_statistics()
{
  dom_stats_.clear();
}

