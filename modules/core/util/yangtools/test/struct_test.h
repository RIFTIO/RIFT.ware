
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

// Test header to verify the pbcms created dynamically
typedef struct RwDynamic__YangData__RwBaseD__Colony RwDynamic__YangData__RwBaseD__Colony;
struct  RwDynamic__YangData__RwBaseD__Colony
{
  ProtobufCMessage base;
  size_t n_trafsim_service;
  RwAppmgrD__YangData__RwBaseD__Colony__TrafsimService **trafsim_service;
  size_t n_bundle_ether;
  RwFpathD__YangData__RwBaseD__Colony__BundleEther **bundle_ether;
  size_t n_port;
  RwFpathD__YangData__RwBaseD__Colony__Port **port;
  RwFpathD__YangData__RwBaseD__Colony__BundleState *bundle_state;
  size_t n_fastpath;
  RwFpathD__YangData__RwBaseD__Colony__Fastpath **fastpath;
  RwFpathD__YangData__RwBaseD__Colony__TrafgenInfo *trafgen_info;
  RwFpathD__YangData__RwBaseD__Colony__PortState *port_state;
  RwFpathD__YangData__RwBaseD__Colony__VirtualFabric *virtual_fabric;
  RwFpathD__YangData__RwBaseD__Colony__IpClassifier *ip_classifier;
  RwFpathD__YangData__RwBaseD__Colony__ExternalAppPlugin *external_app_plugin;
  RwFpathD__YangData__RwBaseD__Colony__LogicalPort *logical_port;
  RwFpathD__YangData__RwBaseD__Colony__LoadBalancer *load_balancer;
  RwFpathD__YangData__RwBaseD__Colony__DistributedClassifier *distributed_classifier;
  RwFpathD__YangData__RwBaseD__Colony__ScriptableLb *scriptable_lb;
  size_t n_network_context_state;
  RwFpathD__YangData__RwBaseD__Colony__NetworkContextState **network_context_state;
  char *name;
  size_t n_network_context;
  RwFpathD__YangData__RwBaseD__Colony__NetworkContext **network_context;
  RwBaseD__YangData__RwBaseD__Colony__Temporary *temporary;
};

static unsigned colony_offset_list[] = {
  offsetof(RwDynamic__YangData__RwBaseD__Colony, n_trafsim_service),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, trafsim_service),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, n_bundle_ether),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, bundle_ether),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, n_port),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, port),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, bundle_state),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, n_fastpath),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, fastpath),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, trafgen_info),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, port_state),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, virtual_fabric),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, ip_classifier),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, external_app_plugin),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, logical_port),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, load_balancer),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, distributed_classifier),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, scriptable_lb),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, n_network_context_state),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, network_context_state),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, name),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, n_network_context),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, network_context),
  offsetof(RwDynamic__YangData__RwBaseD__Colony, temporary),
  sizeof(RwDynamic__YangData__RwBaseD__Colony),
};

typedef struct RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm;
struct RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm
{
  ProtobufCMessage base;
  uint32_t id;
  RwFpathD__YangData__RwBaseD__Vcs__Resources__Vm__Cpu *cpu;
  RwAppmgrD__YangData__RwBaseD__Vcs__Resources__Vm__Memory *memory;
  RwBaseD__YangData__RwBaseD__Vcs__Resources__Vm__Storage *storage;
  size_t n_processes;
  RwBaseD__YangData__RwBaseD__Vcs__Resources__Vm__Processes **processes;
};

static unsigned vm_offset_list[] = {
  offsetof(RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm, id),
  offsetof(RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm, cpu),
  offsetof(RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm, memory),
  offsetof(RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm, storage),
  offsetof(RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm, n_processes),
  offsetof(RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm, processes),
  sizeof(RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm),
};

typedef struct RwDynamic__YangData__RwBaseD__Vcs__Resources RwDynamic__YangData__RwBaseD__Vcs__Resources;
struct RwDynamic__YangData__RwBaseD__Vcs__Resources
{
  ProtobufCMessage base;
  size_t n_vm;
  RwDynamic__YangData__RwBaseD__Vcs__Resources__Vm **vm;
};

static unsigned res_offset_list[] = {
  offsetof(RwDynamic__YangData__RwBaseD__Vcs__Resources, n_vm),
  offsetof(RwDynamic__YangData__RwBaseD__Vcs__Resources, vm),
  sizeof(RwDynamic__YangData__RwBaseD__Vcs__Resources),
};

typedef struct RwDynamic__YangData__RwBaseD__Vcs RwDynamic__YangData__RwBaseD__Vcs;
struct  RwDynamic__YangData__RwBaseD__Vcs
{
  ProtobufCMessage base;
  RwDynamic__YangData__RwBaseD__Vcs__Resources *resources;
};

static unsigned vcs_offset_list[] = {
  offsetof(RwDynamic__YangData__RwBaseD__Vcs, resources),
  sizeof(RwDynamic__YangData__RwBaseD__Vcs),
};

typedef struct RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2 RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2;
struct RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2
{
  ProtobufCMessage base;
  char *appmgr_leaf;
  char *fpath_leaf;
  protobuf_c_boolean has_fpath_binary;
  ProtobufCBinaryData fpath_binary;
  char *deep2l;
};

static unsigned deep2_offset_list[] = {
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2, appmgr_leaf),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2, fpath_leaf),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2, has_fpath_binary),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2, fpath_binary),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2, deep2l),
  sizeof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2),
};

typedef struct RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1 RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1;
struct  RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1
{
  ProtobufCMessage base;
  char *deep1l;
  RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1__Deep2 *deep2;
};

static unsigned deep1_offset_list[] = {
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1, deep1l),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1, deep2),
  sizeof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1),
};

typedef struct RwDynamic__YangData__RwBaseD__MytestCont__Mylist RwDynamic__YangData__RwBaseD__MytestCont__Mylist;
struct  RwDynamic__YangData__RwBaseD__MytestCont__Mylist
{
  ProtobufCMessage base;
  char *name;
  RwDynamic__YangData__RwBaseD__MytestCont__Mylist__Deep1 *deep1;
};

static unsigned mylist_offset_list[] = {
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist, name),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist, deep1),
  sizeof(RwDynamic__YangData__RwBaseD__MytestCont__Mylist),
};

typedef struct RwDynamic__YangData__RwBaseD__MytestCont RwDynamic__YangData__RwBaseD__MytestCont;
struct  RwDynamic__YangData__RwBaseD__MytestCont
{
  ProtobufCMessage base;
  size_t n_mylist;
  RwDynamic__YangData__RwBaseD__MytestCont__Mylist **mylist;
  RwFpathD__YangData__RwBaseD__MytestCont__Basecont *basecont;
  RwAppmgrD__YangData__RwBaseD__MytestCont__Addcont *addcont;
  RwBaseD__YangData__RwBaseD__MytestCont__NoaugPls *noaug_pls;
  char *top_leaf;
};

static unsigned mytest_cont_offset_list[] = {
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont, n_mylist),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont, mylist),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont, basecont),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont, addcont),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont, noaug_pls),
  offsetof(RwDynamic__YangData__RwBaseD__MytestCont, top_leaf),
  sizeof(RwDynamic__YangData__RwBaseD__MytestCont),
};

struct  RwDynamic__YangData__RwBaseD
{
  ProtobufCMessage base;
  size_t n_colony;
  RwDynamic__YangData__RwBaseD__Colony **colony;
  size_t n_node;
  RwFpathD__YangData__RwBaseD__Node **node;
  RwBaseD__YangData__RwBaseD__Tasklet *tasklet;
  RwBaseD__YangData__RwBaseD__Messaging *messaging;
  RwDynamic__YangData__RwBaseD__Vcs *vcs;
  RwBaseD__YangData__RwBaseD__Logging *logging;
  RwBaseD__YangData__RwBaseD__Testlogging *testlogging;
  RwFpathD__YangData__RwBaseD__Keytestlogging *keytestlogging;
  RwBaseD__YangData__RwBaseD__ReturnStatus *return_status;
  RwDynamic__YangData__RwBaseD__MytestCont *mytest_cont;
};


typedef struct RwDynamic__YangInput__RwBaseD__Tracing RwDynamic__YangInput__RwBaseD__Tracing;
struct RwDynamic__YangInput__RwBaseD__Tracing
{
  ProtobufCMessage base;
  protobuf_c_boolean has_appmr_tracing_ip;
  uint32_t appmr_tracing_ip;
  RwAppmgrD__YangInput__RwBaseD__Tracing__AppmrRpcCont *appmr_rpc_cont;
  protobuf_c_boolean has_fpath_tracing_ip;
  uint32_t fpath_tracing_ip;
  RwFpathD__YangInput__RwBaseD__Tracing__FpathRpcCont *fpath_rpc_cont;
  RwBaseD__YangInput__RwBaseD__Tracing__Set *set;
};

static unsigned tracing_offset_list[] = {
  offsetof(RwDynamic__YangInput__RwBaseD__Tracing, has_appmr_tracing_ip),
  offsetof(RwDynamic__YangInput__RwBaseD__Tracing, appmr_tracing_ip),
  offsetof(RwDynamic__YangInput__RwBaseD__Tracing, appmr_rpc_cont),
  offsetof(RwDynamic__YangInput__RwBaseD__Tracing, has_fpath_tracing_ip),
  offsetof(RwDynamic__YangInput__RwBaseD__Tracing, fpath_tracing_ip),
  offsetof(RwDynamic__YangInput__RwBaseD__Tracing, fpath_rpc_cont),
  offsetof(RwDynamic__YangInput__RwBaseD__Tracing, set),
  sizeof(RwDynamic__YangInput__RwBaseD__Tracing),
};

