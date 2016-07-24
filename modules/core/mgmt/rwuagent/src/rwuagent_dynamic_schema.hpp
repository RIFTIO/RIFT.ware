
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwuagent_dynamic_schema.hpp
 * @brief Private micro-agent dynamic schema state machine header
 */

#ifndef CORE_MGMT_RWUAGENT_DYNAMIC_SCHEMA_HPP_
#define CORE_MGMT_RWUAGENT_DYNAMIC_SCHEMA_HPP_

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <vector>

#include "rwdts.h"
#include "rw-mgmt-schema.pb-c.h"
#include "rwdynschema.h"

namespace rw_uagent {

class Instance;

typedef RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchema_Modules) ConfigModule;
typedef RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState) SchemaState;
typedef RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules) DataModule;
typedef RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules_Module) DataModuleContents;
typedef RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps) ListeningApps;

typedef std::lock_guard<std::recursive_mutex> ScopeLock;

struct Module {
  std::string module_name;
  std::string fxs_filename;
  std::string so_filename;
  std::string yang_filename;
  std::string metainfo_filename;
  std::string revision;
  bool exported;

  Module(const std::string& module_name_,
         const std::string& fxs_filename_,
         const std::string& so_filename_,
         const std::string& yang_filename_,
         const std::string& metainfo_filename_,
         const std::string& revision_,
         bool exported_);

};

class PendingLoad
{
 public:
  using ConfigModuleUptr = typename UniquePtrProtobufCMessage<ConfigModule>::uptr_t;

 private:
  PendingLoad() = delete;

  enum State {
    Loading_Apps,
    Loading_Mgmt,
    Loading_Nb,
    Done,
    Error
  };

  std::atomic<State> current_state_{Loading_Apps};
  
  Instance * agent_instance_;
  rwdts_api_t * dts_handle_;
  rwdts_member_reg_handle_t pub_modules_reg_;

  std::vector<Module> batch_;

  std::atomic<bool> complete_{false};

  static void drive_state_machine(void *);
  static void update_state(rwdts_xact_t * xact,
                           rwdts_xact_status_t * xact_status,
                           void * instance_pointer);


  static void retry_load(void *);

  void update_batch_state(RwMgmtSchema__YangEnum__ModuleState__E new_state);

 public:
  PendingLoad(Instance * agent_instance,
              std::vector<Module> batch,
              rwdts_api_t * dts_handle,
              rwdts_member_reg_handle_t pub_modules_reg);
               
  ~PendingLoad() = default;



  static void initiate_load(rwdts_xact_t * xact,
                            rwdts_xact_status_t * xact_status,
                            void * instance_pointer);

  bool is_complete() const noexcept;
};

inline
bool PendingLoad::is_complete() const noexcept
{
  return complete_;
}


/**
 * 
 */
class DynamicSchemaDriver
{
/*
   ATTN: public, then private
 */
 private:
  DynamicSchemaDriver() = delete;

  Instance * agent_instance_ = nullptr;
  rwdts_api_t * dts_handle_  = nullptr;
  rwdts_member_reg_handle_t pub_modules_reg_;
  rwdts_member_reg_handle_t sub_config_reg_;

  std::list<PendingLoad> pending_loads_;
  std::recursive_mutex pending_mutex_;

  bool keep_spinning_ = true;

  void register_publish_modules();
  void register_subscribe_config();
  
  void initiate_module_load();

  static rwdts_member_rsp_code_t config_state_change(const rwdts_xact_info_t * xact_info,
                                                     RWDtsQueryAction action,
                                                     const rw_keyspec_path_t * keyspec,
                                                     const ProtobufCMessage * msg,
                                                     uint32_t credits,
                                                     void * get_next_key);

 public:
  DynamicSchemaDriver(Instance * instance, rwdts_api_t * _dts_handle);
  ~DynamicSchemaDriver() = default;

  static void run(void * instance_pointer);
  void stop();

};


/*
  ATTN: This "class" is not at all self-contained.  there should be an
  instance, and it should keep the agent instance pointer, memlog buffer,
  et cetera...
 */
class AgentDynSchemaHelper
{
private:
  static rw_status_t register_listening_app(Instance* agent_inst);
  
  static rw_status_t register_schema_updates(Instance* agent_inst);
  
  static rw_status_t register_schema_version_info(Instance* agent_inst);

public:
  static rw_status_t register_for_dynamic_schema(Instance* agent_inst);

  static void dynamic_schema_reg_cb(void * app_instance,
                                    int numel,
                                    rwdynschema_module_t * modules);

  static rwdts_member_rsp_code_t dynamic_schema_tasklet_state_cb(
      const rwdts_xact_info_t    *xact_info,
      RWDtsQueryAction            action,
      const rw_keyspec_path_t    *key,
      const ProtobufCMessage     *msg,
      uint32_t                    credits,
      void                       *getnext_ptr);

  static rwdts_member_rsp_code_t dynamic_schema_version_cb(
      const rwdts_xact_info_t    *xact_info,
      RWDtsQueryAction            action,
      const rw_keyspec_path_t    *key,
      const ProtobufCMessage     *msg,
      uint32_t                    credits,
      void                       *getnext_ptr);
 public:
  static rwdynschema_dynamic_schema_registration_t *dyn_schema_reg_h_;
  static std::mutex schema_lck_;
};


}


#endif

