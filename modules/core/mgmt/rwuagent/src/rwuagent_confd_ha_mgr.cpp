/* STANDARD_RIFT_IO_COPYRIGHT */
/*
* @file rwuagent_confd_ha_mgr.cpp
*
* Management Confd HA manager
*/

#include <confd_ha.h>
#include "rwuagent_mgmt_system.hpp"

using namespace rw_uagent;

// static function callbacks
void ConfdHAMgr::set_confd_ha_state_cb(void* ctx)
{
  RW_ASSERT (ctx);
  static_cast<ConfdHAMgr*>(ctx)->set_confd_ha_state();
}

void ConfdHAMgr::update_master_confd_ip_cb(void* ctx)
{
  RW_ASSERT (ctx);
  static_cast<ConfdHAMgr*>(ctx)->update_master_confd_ip();
}

void ConfdHAMgr::active_mgmt_info_cb(const char* active_mgmt_ip,
                                     const char* instance_name,
                                     void *ud)
{
  RW_ASSERT (active_mgmt_ip);
  RW_ASSERT (instance_name);
  RW_ASSERT (ud);

  auto self = static_cast<ConfdHAMgr*>(ud);
  self->master_info_.node_id = instance_name;
  self->master_info_.ip = active_mgmt_ip;

  std::string log("New master info: ");
  log += self->master_info_.node_id + " : " + self->master_info_.ip;
  RW_MA_INST_LOG (self->instance_, InstanceCritInfo, log.c_str());

  self->on_ha_state_change_event(CHANGE_MASTER);
}

// public member definitions

ConfdHAMgr::ConfdHAMgr(ConfdMgmtSystem* mgmt,
                       Instance* instance):
  mgmt_(mgmt),
  instance_(instance),
  memlog_buf_(
      instance_->get_memlog_inst(),
      "ConfdHAMgr",
      reinterpret_cast<intptr_t>(this))
{
}

ConfdHAMgr::~ConfdHAMgr()
{
  if (ha_sock_ >= 0) close(ha_sock_);
}

rw_status_t ConfdHAMgr::ha_init()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "ha init");

  if (ha_sock_ >= 0) close (ha_sock_);

  ha_sock_ = socket(instance_->confd_addr_->sa_family, SOCK_STREAM, 0);
  RW_ASSERT (ha_sock_ >= 0);

  auto ret = confd_ha_connect(ha_sock_, 
                              instance_->confd_addr_, 
                              instance_->confd_addr_size_,
                              "shared-secret-token");

  if (ret != CONFD_OK) {
    std::string log = "HA socket connection failed. Retrying. " + std::string(confd_lasterr());
    RW_MA_INST_LOG (instance_, InstanceError, log.c_str());
    RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "failed confd_ha_connect");

    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

bool ConfdHAMgr::has_master_details() noexcept
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "has master details");

  if (!master_info_.node_id.length() ||
      !master_info_.ip.length()) {
    RW_MA_INST_LOG (instance_, InstanceError, "Master mgmt details have not been received. Retry.");
    return false;
  }
  return true;
}

void ConfdHAMgr::subscribe_for_master_mgmt_info()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "subscribe master mgmt info");
  if (instance_->is_ha_master()) return;

  RW_MA_INST_LOG (instance_, InstanceDebug, "Agent subscribing for the master mgmt info");
 
  rwha_api_register_active_mgmt_info(instance_->dts_api(),
                                      ConfdHAMgr::active_mgmt_info_cb,
                                      this);
}

void ConfdHAMgr::on_ha_state_change_event(HAEventType event)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "on_ha_state_change_event",
      RWMEMLOG_ARG_PRINTF_INTPTR("event = %" PRIu64, event));

  switch (event) {
  case STANDBY_TO_MASTER_TRANSITION:
  {
    RW_MA_INST_LOG (instance_, InstanceCritInfo, "Received standby to master transition event");
    instance_->start_messaging_client();
    mgmt_->standby_to_master_transition();
    break;
  }
  case CHANGE_MASTER:
    RW_MA_INST_LOG (instance_, InstanceCritInfo, "Received change master details event");
    update_master_confd_ip();
    break;
  default:
    RW_ASSERT_NOT_REACHED();
  };
}

void ConfdHAMgr::update_master_confd_ip()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "update master confd ip");
  auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 2);

  // If new master is the current host, then ignore this
  // spurious event
  if (mgmt_->hostname() == master_info_.node_id) {
    return;
  }

  rw_status_t st = ha_init();
  if (st != RW_STATUS_SUCCESS) {
    rwsched_dispatch_after_f(
          instance_->rwsched_tasklet(),
          when,
          rwsched_dispatch_get_main_queue(instance_->rwsched()),
          this,
          update_master_confd_ip_cb);
    return;
  }

  if (!make_confd_standby()) {
    RW_MA_INST_LOG (instance_, InstanceError, 
        "Failed to update details of master mgmt VM. Retrying.");

    rwsched_dispatch_after_f(
        instance_->rwsched_tasklet(),
        when,
        rwsched_dispatch_get_main_queue(instance_->rwsched()),
        this,
        update_master_confd_ip_cb);
    return;
  }
}

// state machine call
void ConfdHAMgr::set_confd_ha_state()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "set confd ha state");
  bool failed = false;

  if (instance_->is_ha_master()) {
    if (!make_confd_master()) failed = true;
  } else {
    if (!make_confd_standby()) failed = true;
  }

  if (failed) {
    auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 2);
    rwsched_dispatch_after_f(
         instance_->rwsched_tasklet(),
         when,
         rwsched_dispatch_get_main_queue(instance_->rwsched()),
         this,
         set_confd_ha_state_cb);
    return;
  }

  RW_MA_INST_LOG (instance_, InstanceInfo, "Confd HA state configured successfully");
  mgmt_->proceed_to_next_state();
}

// private member definitions

bool ConfdHAMgr::make_confd_master()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "make confd master");
  ha_init();
  confd_value_t nodeid;
  //TODO: The line perhaps needs to be uncommented to make use
  // of component names when HA manager is ready.
  //std::string str{instance_->rwtasklet()->rwvcs->identity.rwvm_name};
  std::string str(mgmt_->hostname());

  CONFD_SET_BUF(&nodeid, (unsigned char*)&str[0], str.length());

  int ret = confd_ha_bemaster(ha_sock_, &nodeid);
  if (ret != CONFD_OK) {
    std::string tmp_log = "Failed to make confd node as master. Retry. " +
      std::string(confd_lasterr());
    RW_MA_INST_LOG (instance_, InstanceError, tmp_log.c_str());
    return false;
  }

  return true;
}

bool ConfdHAMgr::make_confd_standby()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "make confd standby");
  confd_value_t nodeid;
  confd_ha_node master;

  std::string str{instance_->rwtasklet()->rwvcs->identity.rwvm_name};

  CONFD_SET_BUF(&nodeid, (unsigned char*)&str[0], str.length());

  master.af = AF_INET;
  CONFD_SET_BUF(&master.nodeid, (unsigned char*)&master_info_.node_id[0], 
      master_info_.node_id.length());

  const char *master_ip = master_info_.ip.c_str();
  int ret = inet_pton(AF_INET, master_ip, &master.addr.ip4);
  if (!ret) {
    std::string log = std::strerror(errno);
    RW_MA_INST_LOG (instance_, InstanceError, log.c_str());
    return false;
  }

  ret = confd_ha_beslave(ha_sock_, &nodeid, &master, 0/*async copy from master*/);

  if (ret != CONFD_OK) {
    std::string log = "Failed to make confd node as standby. Retry. " +
      std::string(confd_lasterr());
    RW_MA_INST_LOG (instance_, InstanceError, log.c_str());
    return false;
  }

  // Check the status
 struct confd_ha_status status;
 ret = confd_ha_status(ha_sock_, &status);
 if (ret != CONFD_OK || status.state == CONFD_HA_STATE_NONE) {
   RW_MA_INST_LOG (instance_, InstanceCritInfo, "Confd not configured as slave. Retrying.");
   return false;
 }
  return true;
}
