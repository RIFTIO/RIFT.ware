/* STANDARD_RIFT_IO_COPYRIGHT */
/*
* @file rwuagent_mgmt_system.cpp
*
* Management agent startup phase handler
*/
#include <sstream>
#include <chrono>
#include <fstream>

#include "rwuagent.hpp"
#include "rw-vcs.pb-c.h"
#include "rw-base.pb-c.h"
#include "reaper_client.h"

using namespace rw_uagent;

MgmtSystemFactory& MgmtSystemFactory::get()
{
  static MgmtSystemFactory plugin_;
  return plugin_;
}

bool MgmtSystemFactory::register_nb(const std::string& id, IPluginRegister* reg)
{
  return plugin_map_.insert(
      std::make_pair(id, reg)).second;
}

bool MgmtSystemFactory::is_registered(const std::string& id) const
{
  return plugin_map_.find(id) != plugin_map_.end();
}

std::unique_ptr<BaseMgmtSystem>
MgmtSystemFactory::create_object(const std::string& id, Instance* instance)
{
  RW_ASSERT (instance);
  if (plugin_map_.find(id) == plugin_map_.end()) {
    return nullptr;
  }

  auto cons = plugin_map_[id];
  return std::unique_ptr<BaseMgmtSystem>(cons->CreateObject(instance));
}

BaseMgmtSystem::BaseMgmtSystem(Instance* inst,
                               const char* trace_name)
  : instance_(inst),
    memlog_buf_(
        inst->get_memlog_inst(),
        trace_name,
        reinterpret_cast<intptr_t>(this) )
{
}

void BaseMgmtSystem::system_startup_cb(void* ctx)
{
  auto* self = static_cast<BaseMgmtSystem*>(ctx);
  RW_ASSERT(self);

  if (!self->system_startup()) {
    const auto five_seconds = NSEC_PER_SEC * 5LL;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, five_seconds);
    rwsched_dispatch_after_f(
        self->instance_->rwsched_tasklet(),
        when,
        rwsched_dispatch_get_main_queue(self->instance_->rwsched()),
        self,
        &system_startup_cb);
    return;
  }

  if (self->instance_->is_ha_master()) {
    self->instance_->start_messaging_client();
    self->instance_->wait_for_critical_tasklets();
  } else {
    // No need to wait for others in standby mode
    // Agent has no configuration to share with others
    self->instance_->dts()->do_manifest_registration();
    self->instance_->subscribe_for_vm_state_notification();
    Instance::tasks_ready_cb(self->instance_);
  }

  return;
}
