
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __RWMAIN_H__
#define __RWMAIN_H__

#include <rw_tasklet_plugin.h>
#include <rwdts.h>
#include <rwvcs.h>
#include <rwvx.h>
#include <time.h>
#include "rwmain_gi.h"

#include "serf_client.h"

__BEGIN_DECLS

#define RWTRACE_CATEGORY_RWMAIN RWTRACE_CATEGORY_RWTASKLET

#define rwmain_trace_info(__rwmain, __fmt, ...) \
  do { \
    RWTRACE_INFO( \
        (__rwmain)->rwvx->rwtrace, \
        RWTRACE_CATEGORY_RWMAIN, \
        "%s-%d: " __fmt, \
        (__rwmain)->component_name, \
        (__rwmain)->instance_id, \
        ##__VA_ARGS__); \
  } while (false);

#define rwmain_trace_crit(__rwmain, __fmt, ...) \
  do { \
    RWTRACE_CRIT( \
        (__rwmain)->rwvx->rwtrace, \
        RWTRACE_CATEGORY_RWMAIN, \
        "%s-%d: " __fmt, \
        (__rwmain)->component_name, \
        (__rwmain)->instance_id, \
        ##__VA_ARGS__); \
  } while (false);

#define rwmain_trace_crit_info(__rwmain, __fmt, ...) \
  do { \
    RWTRACE_CRITINFO( \
        (__rwmain)->rwvx->rwtrace, \
        RWTRACE_CATEGORY_RWMAIN, \
        "%s-%d: " __fmt, \
        (__rwmain)->component_name, \
        (__rwmain)->instance_id, \
        ##__VA_ARGS__); \
  } while (false);


#define rwmain_trace_error(__rwmain, __fmt, ...) \
  do { \
    RWTRACE_ERROR( \
        (__rwmain)->rwvx->rwtrace, \
        RWTRACE_CATEGORY_RWMAIN, \
        "%s-%d: " __fmt, \
        (__rwmain)->component_name, \
        (__rwmain)->instance_id, \
        ##__VA_ARGS__); \
  } while (false);

#define rwmain_trace_warn(__rwmain, __fmt, ...) \
  do { \
    RWTRACE_WARN( \
        (__rwmain)->rwvx->rwtrace, \
        RWTRACE_CATEGORY_RWMAIN, \
        "%s-%d: " __fmt, \
        (__rwmain)->component_name, \
        (__rwmain)->instance_id, \
        ##__VA_ARGS__); \
  } while (false);


#define RWMAIN_DEREG_PATH_DTS(t_apih, t_component, t_recovery) {\
  char *path; \
  int r = asprintf(&path, "/R/%s/%lu", \
                   (t_component)->component_name, \
                   (t_component)->instance_id); \
  RW_ASSERT(r != -1); \
  rw_status_t dereg_status = rwdts_member_deregister_path ((t_apih), path, t_recovery); \
  RW_ASSERT (dereg_status == RW_STATUS_SUCCESS); \
  RW_FREE(path); \
}



typedef RWPB_T_MSG(RwBase_ProcHeartbeatStats)                 rwproc_heartbeat_stat;
typedef RWPB_T_MSG(RwBase_ProcHeartbeatStats_Timing)          rwproc_heartbeat_timing;
typedef RWPB_T_MSG(RwBase_ProcHeartbeatStats_MissedHistogram) rwproc_heartbeat_histogram;
#define rwproc_heartbeat_stat__init(x)        RWPB_F_MSG_INIT(RwBase_ProcHeartbeatStats, x);
#define rwproc_heartbeat_timing__init(x)      RWPB_F_MSG_INIT(RwBase_ProcHeartbeatStats_Timing, x);
#define rwproc_heartbeat_histogram__init(x)   RWPB_F_MSG_INIT(RwBase_ProcHeartbeatStats_MissedHistogram, x);



// Defined in heartbeat.c
struct rwproc_heartbeat;


struct rwmain_tasklet {
  rw_sklist_element_t _sklist;

  char * instance_name;
  uint32_t instance_id;
  RwTaskletPluginComponent *plugin_klass;
  RwTaskletPluginComponentIface *plugin_interface;
  RwTaskletPluginComponentHandle *h_component;
  RwTaskletPluginInstanceHandle *h_instance;

  struct rwmain_gi * rwmain;
  rwsched_CFRunLoopTimerRef kill_if_not_running;

  struct rwtasklet_info_s * tasklet_info;
};

struct rwmain_proc {
  rw_sklist_element_t _sklist;

  char * instance_name;
  pid_t pid;
  bool is_rwproc;

  struct rwmain_gi * rwmain;
  rwsched_CFRunLoopSourceRef cfsource;
};

struct rwmain_multivm {
  rw_sklist_element_t _sklist;

  char key[99];
};


/*
 * Parse the command line and perform the setup of a rwvx instance.
 *
 * @param argc  - argc as passed to main()
 * @param argv  - argv as passed to main()
 * @param envp  - envp as passed to main()
 * @return      - fully initialized rwmain instance
 */
struct rwmain_gi * rwmain_setup(int argc, char ** argv, char ** envp);

/*
 * Bootstrap the current component.  The bootstrap phase is defined in the
 * static manifest and only refers to other components defined in the static
 * manifest.
 *
 * @param rwmain  - rwmain instance
 * @return        - rw_status
 */
rw_status_t rwmain_bootstrap(struct rwmain_gi * rwmain);

rw_status_t rwmain_setup_dts_registrations(struct rwmain_gi * rwmain);
rw_status_t rwmain_setup_dts_rpcs(struct rwmain_gi * rwmain);

/*
 * Setup a timer that will monitor the cpu utilization of both the entire
 * system and this specific process.
 *
 * @param rwmain  - rwmain instance
 */
void rwmain_setup_cputime_monitor(struct rwmain_gi * rwmain);


/*
 * Restart the specified tasklet currently running in this rwvcs instance.
 *
 * @param rwmain            - rwmain instance
 * @param tasklet_instance  - instance-name of tasklet to restart.
 */
rw_status_t rwmain_tasklet_restart(
    struct rwmain_gi * rwmain,
    const char * tasklet_instance);

/*
 * Execute a specified action
 *
 * @param rwmain    - rwmaininstance
 * @param parent_id - instance name of the parent of this component
 * @param action    - action to execute
 * @return          - RW_STATUS_SUCCESS
 *                    aborts on failure
 */
rw_status_t
rwmain_action_run(
    struct rwmain_gi * rwmain,
    const char * parent_id,
   vcs_manifest_action * action);

/*
 * Stop the given instance.  Currently supports stopping either tasklets or rwprocs.
 * Tasklets are stopped by calling the rwtasklet_plugin.instance_stop() function.
 * Rwprocs will kill any tasklets they own and then set the runloop to exit in 5
 * seconds which effectively allows main() to exit.
 *
 * Note that this should only be called from within the rwproc that we wish to stop.
 * It is up to rwmain to pass the message along until the rwmain owning the given
 * rwproc or tasklet is alerted.
 *
 * @param rwmain  - rwmain instance
 * @param id      - component info on instance to stop
 * @return        - RW_STATUS_SUCCESS on success
 */
rw_status_t
rwmain_stop_instance(struct rwmain_gi * rwmain, rw_component_info * id);

/*
 * Initialize the specified VM.  After that point, this process will be
 * considered the endpoint for all communication.
 *
 * @param rwmain          - rwmain instance
 * @param vm_def          - manifest definition of the VM to initialize
 * @param component_name  - name of the VM
 * @param instance_id     - VM instance id
 * @param instance_name   - VM instance name
 * @param parent_id       - Instance name of the parent component if any.  NULL otherwise.
 * @return                - RW_STATUS_SUCCESS
 */
rw_status_t
rwmain_rwvm_init(
    struct rwmain_gi * rwmain,
    vcs_manifest_vm * vm_def,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name,
    const char * parent_id);

/*
 * Initialize the specified process.  After that point, this process will be
 * considered the endpoint for all communication.
 *
 * @param rwmain          - rwmain instance
 * @param proc_def        - manifest definition of the process to initialize
 * @param component_name  - name of the process
 * @param instance_id     - process instance id
 * @param instance_name   - process instance name
 * @param parent_id       - Instance name of the parent component if any.  NULL otherwise.
 * @return                - RW_STATUS_SUCCESS
 */
rw_status_t
rwmain_rwproc_init(
    struct rwmain_gi * rwmain,
    vcs_manifest_proc * proc_def,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name,
    const char * parent_id);

// Duration (seconds) during which missed messages are not counted by the
// heartbeat subscriber.  If no messages have arrived by the end, the process
// is assumed to have crashed.
#define RWVCS_HEARTBEAT_DELAY 600

#define RWVCS_DELAY_START 10000000 // 10 sec 
/*
 * Environment variables:
 *
 * RIFT_PROC_HEARTBEAT_NO_REVERSE:  Disable reverse heartbeating (publisher will not register an error
 *                                  if the subscriber is not keeping up with the message queue).
 */


/*
 * Create a new instance for rwproc heartbeating.
 *
 * @param frequency - number of heartbeats per second
 * @param tolerance - number of consecutive missed heartbeats allowed
 * @return          - rwproc heartbeat instance
 */
struct rwproc_heartbeat * rwproc_heartbeat_alloc(uint32_t frequency, uint32_t tolerance);

/*
 * Free a rwproc_heartbeat instance.  Any heartbeat timers will be canceled.
 *
 * @param rwproc_heartbeat  - instance to free
 */
void rwproc_heartbeat_free(struct rwproc_heartbeat * rwproc_heartbeat);

/*
 * Subscribe to a heartbeat message queue for the given instance.  This is to
 * be used by the owning RWVM to make sure that each RWPROC continues to
 * function.  The queue will be checked rwmain->rwproc_heartbeat->frequency
 * times a second and flushed.  If more than rwmain->rwproc_instance->tolerance
 * checks go by without anything being in the queue, the RWPROC will be killed
 * and marked as crashed.
 *
 * The subscriber will wait up to RWVCS_HEARTBEAT_DELAY seconds to receive
 * a single heartbeat from the publisher.  If no messages are received in
 * this timeframe it is assumed the process never started.
 *
 * Until the first message is received from the publisher, the subscriber will
 * continue to check at rwmain->rwproc_heartbeat->frequency but will not
 * increment the missed message counter.
 *
 * @param rwvx            - rwvx instance
 * @param instance_name   - instance name of the process to be monitored.
 * @return                - rw_status
 */
rw_status_t rwproc_heartbeat_subscribe(
    struct rwmain_gi * rwmain,
    const char * instance_name);

/*
 * Publish a heartbeat on the message queue for the given instance.  This will
 * push a message rwmain->rwproc_heartbeat->frequency times a second.
 *
 * @param rwvx            - rwvx instance
 * @param instance_name   - instance name of the process performing the heartbeat
 * @return                - rw_status
 */
rw_status_t rwproc_heartbeat_publish(
    struct rwmain_gi * rwmain,
    const char * instance_name);

/*
 * Change the heartbeat parameters.
 *
 * @param rwmain    - rwmain instance
 * @param frequency - number of heartbeats per second
 * @param tolerance - number of consecutive missed heartbeats allowed
 * @param enabled   - boolean to enable/disable heartbeats
 * @return          - rw_status
 */
rw_status_t rwproc_heartbeat_reset(
    struct rwmain_gi * rwmain,
    uint16_t frequency,
    uint32_t tolerance,
    bool enabled);

/*
 * Retrieve heartbeat parameters
 *
 * @param rwmain    - rwmain instance
 * @param frequency - on return, number of heartbeats per second
 * @param tolerance - on return, number of consecutive missed heartbeats allowed
 * @param enabled   - on return, whether heartbeats are enabled or not
 */
void rwproc_heartbeat_settings(
    struct rwmain_gi * rwmain,
    uint16_t * frequency,
    uint32_t * tolerance,
    bool * enabled);

/*
 * Retrieve heartbeat statistics.  On completion, the structure pointed to by
 * stats will be filled with any information that this rwmain instance has
 * access to.  It is up to the caller to free the structure(s) on completion.
 *
 * @param rwmain  - rwmain instance
 * @param stats   - on success, an pointer to a newly allocated list filled with
 *                  rwproc heartbeat stats
 * @param n_stats - on success, number of stats put in the stats list.
 * @return        - rw_status
 */
rw_status_t rwproc_heartbeat_stats(
    struct rwmain_gi * rwmain,
    rwproc_heartbeat_stat *** stats,
    size_t * n_stats);

/*
 * Allocate a rwmain tasklet container
 *
 * @param instance_name - tasklet instance name
 * @param instance_id   - tasklet instance id
 * @param mode_active   - tuple indicate presence of mode_active and its value
 * @param plugin_name   - name of plugin defining the tasklet
 * @param plugin_dir    - directory where the plugin can be found
 * @param rwvcs         - rwvcs instance
 * @return              - a rwmain tasklet container representing an idle tasklet.
 */
struct rwmain_tasklet * rwmain_tasklet_alloc(
    const char * instance_name,
    uint32_t instance_id,
    rwmain_tasklet_mode_active_t *mode_active,
    const char * plugin_name,
    const char * plugin_dir,
    rwvcs_instance_ptr_t rwvcs);

/*
 * Stop and free any resources used by a tasklet
 *
 * @param rt  - rwmain_tasklet to stop and free
 */
void rwmain_tasklet_free(struct rwmain_tasklet * rt);

/*
 * Start the tasklet defined by the specified rwmain tasklet container
 *
 * @param rwmain  - rwmain instance
 * @param rt      - rwmain tasklet
 * @return        - rw_status
 */
rw_status_t rwmain_tasklet_start(
    struct rwmain_gi * rwmain,
    struct rwmain_tasklet * rt);

/*
 * Allocate a rwmain native process container
 *
 * @param rwmain        - rwmain instance
 * @param instance_name - proccess instance-name
 * @param is_rwproc     - True if the proc is of type RWPROC
 * @param pid           - process pid
 * @param pipe_fd       - read side of pipe opened with the process.  This is
 *                        used as a cheap form of process monitoring.  If the
 *                        pipe gets closed, the parent will assume the child
 *                        died.
 * @return              - rwmain process container
 */
struct rwmain_proc * rwmain_proc_alloc(
    struct rwmain_gi * rwmain,
    const char * instance_name,
    bool is_rwproc,
    pid_t pid,
    int pipe_fd);

/*
 * Stop and free any resources used by a native-process
 *
 * @param rp  - rwmain_proc to stop and free
 */
void rwmain_proc_free(struct rwmain_proc * rp);

#define VCS_INSTANCE_IMPL 1
#define VCS_INSTANCE_XPATH_FMT "D,/rw-base:vcs/instances/instance[instance-name='%s']"
#define VCS_GET(t_rwmain) (t_rwmain)->rwvx->rwvcs
rw_status_t rwmain_dts_register_vcs_instance(
    struct rwmain_gi * rwmain,
    const char* instance_name);
rw_status_t rwvcs_instance_update_child_state(
    struct rwmain_gi *rwmain,
    char *child_name,
    const char *parent_name,
    RwvcsTypes__YangEnum__ComponentType__E component_type,
    RwBase__YangEnum__AdminCommand__E admin_state);

rw_status_t rwvcs_instance_delete_child(
    struct rwmain_gi *rwmain,
    char *child_name,
    const char *parent_name,
    RwvcsTypes__YangEnum__ComponentType__E component_type);

rw_status_t rwvcs_instance_delete_instance(
    struct rwmain_gi *rwmain);

rw_status_t 
rwmain_dts_config_ready_process(rwvcs_instance_ptr_t rwvcs,
                                rw_component_info * id);
rw_status_t send2dts_start_req(
  struct rwmain_gi * rwmain,
  const rwdts_xact_info_t * xact_info,
  vcs_rpc_start_input *start_req);

rw_status_t send2dts_stop_req(
  struct rwmain_gi * rwmain,
  const rwdts_xact_info_t * xact_info,
  char *instance_name,
  char *child_name);

rwdts_member_rsp_code_t do_vstart(
    const rwdts_xact_info_t* xact_info,
    struct rwmain_gi * rwmain,
    rw_vcs_instance_childn *child_n,
    char *parent_instance,
    char **child_instance);

rwdts_member_rsp_code_t do_vstop(
    const rwdts_xact_info_t * xact_info,
    struct rwmain_gi * rwmain,
    char *stop_instance_name);

rwdts_member_rsp_code_t do_vstop_new(
    const rwdts_xact_info_t * xact_info,
    struct rwmain_gi * rwmain,
    char *stop_instance_name);
int main_function(int argc, char ** argv, char ** envp);

/*
 * Send a response to a start request.
 *
 * @param xact          - dts transaction
 * @param query         - dts query
 * @param key           - dts key
 * @param start_status  - result of the start request
 * @param instance_name - instance name of the launched instance.  If no instance was launched, pass NULL.
 */
void send_start_request_response(
  rwdts_xact_t * xact,
  rwdts_query_handle_t query,
  rw_status_t start_status,
  const char * instance_name);

struct dts_start_stop_closure {
  struct rwmain_gi * rwmain;
  rwdts_xact_t * xact;
  rwdts_query_handle_t query;

  // start: parent instance name
  // stop:  instance to stop
  char * instance_name;

  // start: component-name
  char * rpc_query;
  char * ip_addr;
  rw_admin_command admin_command;

  // corrilation-id for start-req
  uint32_t vstart_corr_id;
};

void init_phase(rwsched_CFRunLoopTimerRef timer, void * ctx);
struct rwmain_gi * rwmain_alloc(
    rwvx_instance_ptr_t rwvx,
    const char * component_name,
    uint32_t instance_id,
    const char * component_type,
    const char * parent_id,
    const char * vm_ip_address,
    uint32_t vm_instance_id);

/*
 * Create a start action and pass it to rwvcs to start an instance of an component
 *
 * @param rwmain          - rwmaininstance
 * @param component_name  - name of the component to start
 * @return                - RW_STATUS_SUCCESS on success, status from rwmain_action_run() otherwise
 */
rw_status_t start_component(
    struct rwmain_gi * rwmain,
    const char * component_name,
    const char * ip_addr,
    rw_admin_command admin_command,
    const char * parent_instance,
    char ** instance_name,
    vcs_manifest_component *m_component);

typedef enum rwmain_pm_ip_enum_s {
  RWMAIN_PM_IP_STATE_INVALID,
  RWMAIN_PM_IP_STATE_FAILED,
  RWMAIN_PM_IP_STATE_ACTIVE,
  RWMAIN_PM_IP_STATE_LEADER
} rwmain_pm_ip_enum_t;

typedef struct rwmain_pm_ip_entry_s {
  char *pm_ip_addr;
  rwmain_pm_ip_enum_t pm_ip_state;
} rwmain_pm_ip_entry_t;

#define RWVCS_ZK_SERVER_CLUSTER 3
typedef struct rwmain_pm_struct_s {
  bool i_am_dc;
  rwmain_pm_ip_entry_t ip_entry[RWVCS_ZK_SERVER_CLUSTER];
} rwmain_pm_struct_t;

rw_status_t
rwmain_pm_handler(
    struct rwmain_gi *rwmain,
    rwmain_pm_struct_t *rwmain_pm);

rw_status_t  process_component_death(
    rwmain_gi_t * rwmain,
    char *instance_name,
    rw_component_info *cinfo);
void handle_recovery_action_instance_name(
    rwmain_gi_t * rwmain,
    char *instance_name);
void restart_process(
    rwmain_gi_t * rwmain,
    char *instance_name);

void start_pacemaker_and_determine_role(
    rwvx_instance_ptr_t rwvx,
    vcs_manifest       *pb_rwmanifest,
    rwmain_pm_struct_t *rwmain_pm);

rw_status_t
read_pacemaker_state(rwmain_pm_struct_t *pm);

#define RWMAIN_SKIP_COMPONENTS(t_cinfo, t_failed_vm) \
    (!strcmp((t_cinfo).component_name, "dtsrouter") \
     ||!strcmp((t_cinfo).component_name, "msgbroker") \
     ||!strcmp((t_cinfo).component_name, "logd") \
     ||!strcmp((t_cinfo).component_name, "RW.TermIO") \
     || strstr((t_cinfo).instance_name, "confd") \
     || strstr((t_cinfo).instance_name, t_failed_vm))

typedef struct rwmain_restart_instance_s {
  char *instance_name;
  struct rwmain_restart_instance_s *next_restart_instance;
  struct rwmain_gi *rwmain;
  bool no_skip;
} rwmain_restart_instance_t;

void
rwmain_restart_deferred(rwmain_restart_instance_t *restart_list);

void kill_component(
    rwmain_gi_t * rwmain,
    char *instance_name,
    rw_component_info *ci);
__END_DECLS
#endif
