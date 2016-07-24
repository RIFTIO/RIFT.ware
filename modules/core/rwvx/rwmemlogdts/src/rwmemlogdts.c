/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 * Creation Date: 9/11/15
 * 
 */
#include <stdio.h>

#include <rwmemlogdts.h>
#include <rwmemlog.h>
#include <rwmemlog_mgmt.h>
#include <rw-memlog.pb-c.h>

#include <rwdts.h>
#include <rwdts_api_gi.h>
#include <rwdts_appconf_api.h>
#include <rwdts_member_api.h>


static rwdts_member_rsp_code_t rwmemlogdts_dts_callback_state(
  const rwdts_xact_info_t * xact_info,
  RWDtsQueryAction action,
  const rw_keyspec_path_t* key_in,
  const ProtobufCMessage * msg,
  uint32_t credits,
  void *getnext_ptr )
{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwmemlog_instance_t *instance = (rwmemlog_instance_t *) xact_info->ud;

  rw_keyspec_path_t* output_ks = NULL;
  RWPB_T_MSG(RwMemlog_InstanceState)* output_msg = NULL;

  rw_status_t rs = rwmemlog_instance_get_output(
    instance,
    key_in,
    &output_ks,
    &output_msg );

  if (RW_STATUS_SUCCESS != rs) {
    return RWDTS_ACTION_NA;
  }

  rwdts_xact_info_respond_keyspec(
    xact_info,
    RWDTS_XACT_RSP_CODE_ACK,
    output_ks,
    &output_msg->base );

  rw_keyspec_instance_t *ks_instance = (rw_keyspec_instance_t *)instance->ks_instance;
  rw_keyspec_path_free(output_ks, ks_instance);
  protobuf_c_message_free_unpacked(
    ks_instance ? ks_instance->pbc_instance : NULL,
    &output_msg->base );

  return RWDTS_ACTION_OK;
}


static rwdts_member_rsp_code_t rwmemlogdts_dts_callback_command(
  const rwdts_xact_info_t * xact_info,
  RWDtsQueryAction action,
  const rw_keyspec_path_t* key_in,
  const ProtobufCMessage * msg,
  uint32_t credits,
  void *getnext_ptr )
{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwmemlog_instance_t *instance = (rwmemlog_instance_t *) xact_info->ud;
  RWPB_M_MSG_DECL_CAST(RwMemlog_Command, command, msg);

#define RWMEMLOG_MATCHES_INSTANCE( inst_, msg_ ) \
  ( \
      (   NULL == (msg_)->tasklet_name \
       || (   (inst_)->tasklet_info \
           && 0 == strcmp( rwtasklet_info_get_instance_name((inst_)->tasklet_info), \
                           (msg_)->tasklet_name ) ) ) \
   && (   NULL == (msg_)->instance_name \
       || 0 == strcmp( (msg_)->instance_name, (inst_)->instance_name ) ) \
  )

  if (   (   command->clear
          && RWMEMLOG_MATCHES_INSTANCE( instance, command->clear ))
      || (   command->keep
          && RWMEMLOG_MATCHES_INSTANCE( instance, command->keep )) ) {
    rw_status_t rs = rwmemlog_instance_command( instance, command );
    if (RW_STATUS_SUCCESS == rs) {
      return RWDTS_ACTION_OK;
    }
  }
  return RWDTS_ACTION_NA;
}


static void rwmemlogdts_dts_callback_config_prepare(
  rwdts_api_t *dts_api,
  rwdts_appconf_t *ac,
  rwdts_xact_t *xact,
  const rwdts_xact_info_t *queryh,
  rw_keyspec_path_t *keyspec,
  const ProtobufCMessage *pbmsg,
  void *ctx,
  void *scratch_in)
{
  (void)dts_api;
  (void)keyspec;
  (void)scratch_in;

  RWPB_M_MSG_DECL_CAST(RwMemlog_InstanceConfig, config, pbmsg);
  rwmemlog_instance_t* instance = (rwmemlog_instance_t*)ctx;

  char errorbuf[256];

  rw_status_t reason = rwmemlog_instance_config_validate(
    instance,
    config,
    errorbuf,
    sizeof(errorbuf) );
  if (RW_STATUS_SUCCESS != reason) {
    rwdts_appconf_prepare_complete_fail( ac, queryh, reason, errorbuf );
    return;
  }

  rwdts_appconf_prepare_complete_ok(ac, queryh);
  return;
}


static void rwmemlogdts_dts_callback_config_apply(
  rwdts_api_t *dts_api,
  rwdts_appconf_t *ac,
  rwdts_xact_t *xact,
  rwdts_appconf_action_t action,
  void *ctx,
  void *scratch_in)
{
  (void)dts_api;
  (void)ac;
  (void)action;
  (void)scratch_in;

  rwmemlog_instance_t* instance = (rwmemlog_instance_t*)ctx;

  rwdts_member_cursor_t *cursor
    = rwdts_member_data_get_current_cursor(xact, instance->dts_config_reg);
  const ProtobufCMessage* pbmsg 
    = rwdts_member_reg_handle_get_next_element(
        instance->dts_config_reg, cursor, xact, NULL);
  if (pbmsg) {
    RWPB_M_MSG_DECL_CAST( RwMemlog_InstanceConfig, config, pbmsg );
    rw_status_t reason = rwmemlog_instance_config_apply( instance, config );
    RW_ASSERT(RW_STATUS_SUCCESS == reason);
  }
}


rw_status_t rwmemlog_instance_dts_register(
  rwmemlog_instance_t* instance,
  rwtasklet_info_ptr_t tasklet_info,
  rwdts_api_t* dts_api )
{
  RW_ASSERT(instance);
  if (!instance) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(!instance->tasklet_info);
  if (instance->tasklet_info) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(!instance->dts_api);
  if (instance->dts_api) {
    return RW_STATUS_FAILURE;
  }

  instance->dts_api = dts_api;

  char* tasklet_name = NULL;
  if (tasklet_info) {
    tasklet_name = rwtasklet_info_get_instance_name(tasklet_info);
    instance->tasklet_info = tasklet_info;
    rwtasklet_info_ref(tasklet_info);
  }
  if (!tasklet_name) {
    tasklet_name = RW_STRDUP(rwdts_api_client_path(dts_api));
  }
  if (!tasklet_name) {
    tasklet_name = RW_STRDUP("unknown");
  }

  /*
   * Setup state handler.
   */
  RWPB_M_PATHSPEC_DECL_INIT(RwMemlog_InstanceState, keyspec_state);
  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*)&keyspec_state;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_state.dompath.path001.has_key00 = 1;
  keyspec_state.dompath.path001.key00.tasklet_name = RW_STRDUP(tasklet_name);

  keyspec_state.dompath.path002.has_key00 = 1;
  keyspec_state.dompath.path002.key00.instance_name = RW_STRDUP(instance->instance_name);

  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.cb.prepare = rwmemlogdts_dts_callback_state;
  reg_cb.ud = (void *)instance;

  instance->dts_reg_state = rwdts_member_register(
    NULL,
    dts_api,
    keyspec,
    &reg_cb,
    RWPB_G_MSG_PBCMD(RwMemlog_InstanceState),
    RWDTS_FLAG_PUBLISHER,
    NULL);
  RW_FREE(keyspec_state.dompath.path002.key00.instance_name);
  keyspec_state.dompath.path002.key00.instance_name = NULL;
  RW_FREE(keyspec_state.dompath.path001.key00.tasklet_name);
  keyspec_state.dompath.path001.key00.tasklet_name = NULL;
  if (!instance->dts_reg_state) {
    goto error;
  }

  /*
   * Setup rpc handler.
   */
  RWPB_M_PATHSPEC_DECL_INIT(RwMemlog_Command, keyspec_command);
  keyspec = (rw_keyspec_path_t*)&keyspec_command;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_RPC_INPUT);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.cb.prepare = rwmemlogdts_dts_callback_command;
  reg_cb.ud = (void *)instance;

  instance->dts_reg_rpc = rwdts_member_register(
    NULL,
    dts_api,
    keyspec,
    &reg_cb,
    RWPB_G_MSG_PBCMD(RwMemlog_Command),
    RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
    NULL);
  if (!instance->dts_reg_rpc) {
    goto error;
  }

  /*
   * Setup config handler.
   */
  rwdts_appconf_cbset_t cbset = {
    .config_apply = rwmemlogdts_dts_callback_config_apply,
    .ctx = instance,
  };

  instance->dts_config_group = rwdts_appconf_group_create(
    dts_api,
    NULL,
    &cbset);
  if (!instance->dts_config_group){
    goto error;
  }

  RWPB_M_PATHSPEC_DECL_INIT(RwMemlog_InstanceConfig, keyspec_config);
  keyspec = (rw_keyspec_path_t*)&keyspec_config;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);

  keyspec_config.dompath.path001.has_key00 = 1;
  keyspec_config.dompath.path001.key00.tasklet_name = RW_STRDUP(tasklet_name);

  keyspec_config.dompath.path002.has_key00 = 1;
  keyspec_config.dompath.path002.key00.instance_name = RW_STRDUP(instance->instance_name);

  instance->dts_config_reg = rwdts_appconf_register(
    instance->dts_config_group,
    keyspec,
    NULL, /*shard*/
    RWPB_G_MSG_PBCMD(RwMemlog_InstanceConfig),
    ( RWDTS_FLAG_SUBSCRIBER | RWDTS_FLAG_CACHE ),
    &rwmemlogdts_dts_callback_config_prepare );
  RW_ASSERT(instance->dts_config_reg);

  RW_FREE(keyspec_config.dompath.path001.key00.tasklet_name);
  keyspec_config.dompath.path001.key00.tasklet_name = NULL;
  RW_FREE(keyspec_config.dompath.path002.key00.instance_name);
  keyspec_config.dompath.path002.key00.instance_name = NULL;
  RW_FREE(tasklet_name);
  tasklet_name = NULL;
  /* Say we're finished */
  rwdts_appconf_phase_complete(instance->dts_config_group, RWDTS_APPCONF_PHASE_REGISTER);

  return instance->dts_config_reg?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;

error:
  rwmemlog_instance_dts_deregister( instance, false );
  RW_FREE(tasklet_name);
  tasklet_name = NULL;
  return RW_STATUS_FAILURE;
}


void rwmemlog_instance_dts_deregister(
  rwmemlog_instance_t* instance,
  bool dts_internals)
{
  RW_ASSERT(instance->buffer_pool);
  if (!instance->buffer_pool) {
    return;
  }
  RW_ASSERT(instance->mutex);
  if (!instance->mutex) {
    return;
  }

  if (instance->dts_config_group && !dts_internals) {
    // ATTN: How do I delete this thing?
  }
  instance->dts_config_group = NULL;

  if (instance->dts_config_reg && !dts_internals) {
    // ATTN: How do I delete this thing?
  }
  instance->dts_config_reg = NULL;

  if (instance->dts_reg_rpc && !dts_internals) {
    rwdts_member_deregister(instance->dts_reg_rpc);
  }
  instance->dts_reg_rpc = NULL;

  if (instance->dts_reg_state && !dts_internals) {
    rwdts_member_deregister(instance->dts_reg_state);
  }
  instance->dts_reg_state = NULL;

  instance->dts_api = NULL;

  if (instance->tasklet_info) {
    rwtasklet_info_unref(instance->tasklet_info);
  }
  instance->tasklet_info = NULL;

  return;
}

