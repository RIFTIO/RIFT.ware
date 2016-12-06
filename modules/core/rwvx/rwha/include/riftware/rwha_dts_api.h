#include <rwdts.h>
#include <rwvcs.h>

__BEGIN_DECLS

typedef struct rwha_api_vm_state_cb_s {
  void (*cb_fn) (void *ud, vcs_vm_state p_state, vcs_vm_state n_state);
  void *ud;
} rwha_api_vm_state_cb_t;

typedef struct rwha_api_vm_state_info_s {
  vcs_vm_state vm_state;
  char *rwvm_name;
  char *instance_name;
  rwha_api_vm_state_cb_t cb;
} rwha_api_vm_state_info_t;

typedef void (*rwha_api_active_mgmt_info_cb_fn)(const char *active_mgmt_ip, const char *instance_name, void *ud);
typedef struct rwha_api_active_mgmt_info_s {
  rwha_api_active_mgmt_info_cb_fn active_mgmt_info_cb_fn;
  void *ud;
} rwha_api_active_mgmt_info_t;

/*!
 * Register vm state change events
 *
 * @param apih      : DTS Api handle
 * @param vm_state  : vm state if known
 * @param cb        : callback and userdata
 */
/// @cond GI_SCANNER
/**
 * rwha_api_set_keyspec_msg_modechange_info
 * @apih:(transfer none)
 * @vm_state: (optional)
 * @cb:(nullable)(optional)
 *
 */
/// @endcond
void rwha_api_register_vm_state_notification(
    rwdts_api_t *apih,
    vcs_vm_state vm_state,
    rwha_api_vm_state_cb_t *cb);

/*!
 * Register active mgmt info updates
 *
 * @param apih                  : DTS Api handle
 * @param active_mgmt_info_cb_fn: callback function for update
 * @param ud                    : userdata
 */
/// @cond GI_SCANNER
/**
 * rwha_api_set_keyspec_msg_modechange_info
 * @apih:(transfer none)
 * @active_mgmt_info_cb_fn:
 * @ud:
 *
 */
/// @endcond
rw_status_t
rwha_api_register_active_mgmt_info(rwdts_api_t *apih,
                                       rwha_api_active_mgmt_info_cb_fn active_mgmt_info_cb_fn,
                                       void *ud);
/*!
 * Publish ready state from uagent. 
 *
 * @param apih  : DTS Api handle
 * @param ready : readiness state of uAgent
 */
rw_status_t
rwha_api_publish_uagent_state(rwdts_api_t *apih,
                              bool        ready);
/*!
 *
 * @param rwvm_name   : vm name
 * @param instance_name: instance_name
 * @param vm_state     : vm_state to be set
 * @return SUCCESS if succesfully added
 */
/// @cond GI_SCANNER
/**
 * rwha_api_add_modeinfo_query_to_block
 * @block:
 * @rwvm_name:
 * @instance_name:
 * Returns: ProtobufCMessage (transfer full)
 *
 */
/// @endcond
rw_status_t
rwha_api_add_modeinfo_query_to_block(
    rwdts_xact_block_t   *block,
    char *rwvm_name,
    char *instance_name,
    vcs_vm_state vm_state);
__END_DECLS
