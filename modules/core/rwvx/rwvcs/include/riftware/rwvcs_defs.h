
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
 *
 */

#ifndef __RWVCS_DEFS_H__
#define __RWVCS_DEFS_H__

#include <rw-base.pb-c.h>
#include <rw-manifest.pb-c.h>
#include <rwvcs-types.pb-c.h>
#include <rw-vcs.pb-c.h>

__BEGIN_DECLS

/* rw-manifest.yang */

typedef RWPB_T_MSG(RwManifest_Manifest)         vcs_manifest;
typedef RWPB_T_MSG(RwManifest_BootstrapPhase)   vcs_manifest_bootstrap;
typedef RWPB_T_MSG(RwManifest_InitPhase)        vcs_manifest_init;
typedef RWPB_T_MSG(RwManifest_Trace)            vcs_manifest_trace;
typedef RWPB_T_MSG(RwManifest_Inventory)        vcs_manifest_inventory;
typedef RWPB_T_MSG(RwManifest_OpInventory)      vcs_manifest_op_inventory;
typedef RWPB_T_MSG(RwManifest_InitTasklet)      vcs_manifest_init_tasklet;
typedef RWPB_T_MSG(RwManifest_Settings)         vcs_manifest_settings;
typedef RWPB_T_MSG(RwManifest_InitPhase_Rwcal)  vcs_manifest_rwcal;
typedef RWPB_T_MSG(RwManifest_Settings_Rwmsg)   vcs_manifest_rwmsg;
typedef RWPB_T_MSG(RwManifest_Settings_Rwvcs)   vcs_manifest_rwvcs;

RW_STATIC_ASSERT(sizeof(RWPB_T_MSG(RwManifest_data_Manifest_Inventory_Component)) == sizeof(RWPB_T_MSG(RwManifest_data_Manifest_OperationalInventory_Component)));

typedef RWPB_T_MSG(RwManifest_data_Manifest_Inventory_Component)     vcs_manifest_component;
typedef RWPB_T_MSG(RwManifest_VcsRwCollection)  vcs_manifest_collection;
typedef RWPB_T_MSG(RwManifest_VcsRwVm)          vcs_manifest_vm;
typedef RWPB_T_MSG(RwManifest_NativeProc)       vcs_manifest_native_proc;
typedef RWPB_T_MSG(RwManifest_VcsRwproc)        vcs_manifest_proc;
typedef RWPB_T_MSG(RwManifest_VcsRwTasklet)     vcs_manifest_tasklet;

typedef RWPB_T_MSG(RwManifest_VcsEventList) vcs_manifest_event_list;
typedef RWPB_T_MSG(RwManifest_VcsEvent)     vcs_manifest_event;
typedef RWPB_T_MSG(RwManifest_VcsAction)    vcs_manifest_action;
typedef RWPB_T_MSG(RwManifest_ActionStart)  vcs_manifest_action_start;

typedef RWPB_T_MSG(RwManifest_RwVmPool)                           vcs_manifest_vm_pool;
typedef RWPB_T_MSG(RwManifest_RwVmPool_StaticVmList)              vcs_manifest_vm_pool_list;
typedef RWPB_T_MSG(RwManifest_RwVmPool_StaticVmRange)             vcs_manifest_vm_pool_range;
typedef RWPB_T_MSG(RwManifest_RwVmPool_StaticVmRange_VmNameRange) vcs_manifest_vm_pool_name_range;
typedef RWPB_T_MSG(RwManifest_RwVmPool_StaticVmRange_VmIpRange)   vcs_manifest_vm_pool_ip_range;

#define vcs_manifest__init(x)               RWPB_F_MSG_INIT(RwManifest_Manifest, x)
#define vcs_manifest_boostrap__init(x)      RWPB_F_MSG_INIT(RwManifest_BootstrapPhase, x)
#define vcs_manifest_init__init(x)          RWPB_F_MSG_INIT(RwManifest_InitPhase, x)
#define vcs_manifest_trace__init(x)         RWPB_F_MSG_INIT(RwManifest_Trace, x)
#define vcs_manifest_inventory__init(x)     RWPB_F_MSG_INIT(RwManifest_Inventory, x)
#define vcs_manifest_op_inventory__init(x)  RWPB_F_MSG_INIT(RwManifest_OpInventory, x)
#define vcs_manifest_init_tasklet__init(x)  RWPB_F_MSG_INIT(RwManifest_InitTasklet, x)
#define vcs_manifest_settings__init(x)      RWPB_F_MSG_INIT(RwManifest_Settings, x)
#define vcs_manifest_rwcal__init(x)         RWPB_F_MSG_INIT(RwManifest_InitPhase_Rwcal, x)
#define vcs_manifest_rwmsg__init(x)         RWPB_F_MSG_INIT(RwManifest_Settings_Rwmsg, x)
#define vcs_manifest_rwvcs__init(x)         RWPB_F_MSG_INIT(RwManifest_Settings_Rwvcs, x)

#define vcs_manifest_component__init(x)     RWPB_F_MSG_INIT(RwManifest_data_Manifest_Inventory_Component, x)
#define vcs_manifest_collection__init(x)    RWPB_F_MSG_INIT(RwManifest_VcsRwCollection, x)
#define vcs_manifest_vm__init(x)            RWPB_F_MSG_INIT(RwManifest_VcsRwVm, x)
#define vcs_manifest_native_proc__init(x)   RWPB_F_MSG_INIT(RwManifest_VcsNativeProc, x)
#define vcs_manifest_proc__init(x)          RWPB_F_MSG_INIT(RwManifest_VcsRwproc, x)
#define vcs_manifest_tasklet__init(x)       RWPB_F_MSG_INIT(RwManifest_VcsRwTasklet, x)

#define vcs_manifest_event_list__init(x)    RWPB_F_MSG_INIT(RwManifest_VcsEventList, x)
#define vcs_manifest_event__init(x)         RWPB_F_MSG_INIT(RwManifest_VcsEvent, x)
#define vcs_manifest_action__init(x)        RWPB_F_MSG_INIT(RwManifest_VcsAction, x)
#define vcs_manifest_action_start__init(x)  RWPB_F_MSG_INIT(RwManifest_ActionStart, x)

#define vcs_manifest_vm_pool__init(x)             RWPB_F_MSG_INIT(RwManifest_RwVmPool, x)
#define vcs_manifest_vm_pool_list__init(x)        RWPB_F_MSG_INIT(RwManifest_RwVmPool_StaticVmList, x)
#define vcs_manifest_vm_pool_range__init(x)       RWPB_F_MSG_INIT(RwManifest_RwVmPool_StaticVmRange, x)
#define vcs_manifest_vm_pool_name_range__init(x)  RWPB_F_MSG_INIT(RwManifest_RwVmPool_StaticVmRange_VmNameRange, x)
#define vcs_manifest_vm_pool_ip_range__init(x)    RWPB_F_MSG_INIT(RwManifest_RwVmPool_StaticVmRange_VmIpRange, x)


/* rw-base.yang */

typedef RWPB_E(RwBase_StateType)          rw_component_state;
typedef RWPB_E(RwvcsTypes_ComponentType)  rw_component_type;
typedef RWPB_T_MSG(RwBase_ComponentInfo)  rw_component_info;
typedef RWPB_T_MSG(RwBase_CollectionInfo) rw_collection_info;
typedef RWPB_T_MSG(RwBase_VmInfo)         rw_vm_info;
typedef RWPB_T_MSG(RwBase_ProcInfo)       rw_proc_info;
typedef RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)  rw_vcs_instance_childn;
typedef RwBase__YangEnum__AdminCommand__E rw_admin_command;


#define rw_component_info__init(x)        RWPB_F_MSG_INIT(RwBase_ComponentInfo, x);
#define rw_collection_info__init(x)       RWPB_F_MSG_INIT(RwBase_CollectionInfo, x);
#define rw_vm_info__init(x)               RWPB_F_MSG_INIT(RwBase_VmInfo, x);
#define rw_proc_info__init(x)             RWPB_F_MSG_INIT(RwBase_ProcInfo, x);
#define rw_vcsinstance_childn__init(x)    RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance_ChildN, x);


/* rw-vcs.yang */
typedef RWPB_T_MSG(RwVcs_output_Vstop)    vcs_rpc_stop_output;
typedef RWPB_T_MSG(RwVcs_input_Vstop)     vcs_rpc_stop_input;
typedef RWPB_T_MSG(RwVcs_output_Vstart)   vcs_rpc_start_output;
typedef RWPB_T_MSG(RwVcs_input_Vstart)    vcs_rpc_start_input;
typedef RWPB_T_MSG(RwVcs_output_Vcrash)   vcs_rpc_crash_output;
typedef RWPB_T_MSG(RwVcs_input_Vcrash)    vcs_rpc_crash_input;
typedef RWPB_T_MSG(RwVcs_output_Vsnap)    vcs_rpc_snap_output;
typedef RWPB_T_MSG(RwVcs_input_Vsnap)     vcs_rpc_snap_input;

#define vcs_rpc_stop_output__init(x)      RWPB_F_MSG_INIT(RwVcs_output_Vstop, x)
#define vcs_rpc_stop_input__init(x)       RWPB_F_MSG_INIT(RwVcs_input_Vstop, x)
#define vcs_rpc_start_output__init(x)     RWPB_F_MSG_INIT(RwVcs_output_Vstart, x)
#define vcs_rpc_start_input__init(x)      RWPB_F_MSG_INIT(RwVcs_input_Vstart, x)


/* Generic stuff for dealing with yang based protobufs */
#define protobuf_free(x)        protobuf_c_message_free_unpacked(NULL, &(x)->base)
#define protobuf_free_stack(x)  protobuf_c_message_free_unpacked_usebody(NULL, &(x).base)

typedef RwvcsTypes__YangEnum__RecoveryType__E vcs_recovery_type;
typedef RwvcsTypes__YangEnum__DataStore__E data_store_type;
typedef RwvcsTypes__YangEnum__VmState__E vcs_vm_state;
#define RWVCS_RWZK_TIMEOUT_S 90

__END_DECLS
#endif

