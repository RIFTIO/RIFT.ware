/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Creation Date: 9/11/15
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */
#include <glib-object.h>

#include "rwdts.h"
#include "rwdts_api_gi.h"
#include "rwdts_int.h"
#include "rwdts_member_api.h"
#include "rwdts_query_api.h"
#include "rwmemlog.h"
#include "rwmemlog_mgmt.h"

#include "rw-mgmt-schema.pb-c.h"
#include "rw_file_update.h"
#include "rwdynschema.h"
#include "rwdynschema_load_schema.h"

char ** extract_northbound_listing_from_manifest(rwdts_api_t * dts_handle, int* n_northbound_listing)
{
  if (dts_handle->rwtasklet_info && 
      dts_handle->rwtasklet_info->rwvcs &&
      dts_handle->rwtasklet_info->rwvcs->pb_rwmanifest &&
      dts_handle->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase &&
      dts_handle->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt &&
      dts_handle->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->northbound_listing) {

    *n_northbound_listing = dts_handle->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->n_northbound_listing;
    return dts_handle->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->northbound_listing;
  }

  return NULL;
}

static rwdynschema_dynamic_schema_registration_t *
rwdynschema_dynamic_schema_registration_ref(rwdynschema_dynamic_schema_registration_t * boxed);
static void
rwdynschema_dynamic_schema_registration_unref(rwdynschema_dynamic_schema_registration_t * boxed);

G_DEFINE_BOXED_TYPE(rwdynschema_dynamic_schema_registration_t,
                    rwdynschema_dynamic_schema_registration,
                    rwdynschema_dynamic_schema_registration_ref,
                    rwdynschema_dynamic_schema_registration_unref);

rwdynschema_dynamic_schema_registration_t *
rwdynschema_dynamic_schema_registration_ref(rwdynschema_dynamic_schema_registration_t * boxed)
{
  g_atomic_int_inc(&boxed->ref_count);
  return boxed;
}

void
rwdynschema_dynamic_schema_registration_unref(rwdynschema_dynamic_schema_registration_t * boxed)
{
  if (!g_atomic_int_dec_and_test(&boxed->ref_count)) {
    return;
  }

  int i = 0;
  for (; i < boxed->batch_size; ++i) {
    RW_FREE(boxed->modules[i].module_name);
    RW_FREE(boxed->modules[i].fxs_filename);
    RW_FREE(boxed->modules[i].so_filename);
    RW_FREE(boxed->modules[i].yang_filename);
    RW_FREE(boxed->modules[i].metainfo_filename);
  }

  RW_FREE(boxed->modules);
  for (i = 0; i < boxed->n_northbound_listing; ++i) {
    RW_FREE(boxed->northbound_listing[i]);
  }
  RW_FREE(boxed->northbound_listing);
  RW_FREE(boxed->app_name);
  g_free(boxed);
}

rwdynschema_dynamic_schema_registration_t *
rwdynschema_dynamic_schema_registration_new(char const * app_name,
                                            rwdts_api_t * dts_handle,
                                            void * app_instance,
                                            rwdynschema_app_sub_cb app_sub_cb,
                                            RwdynschemaAppType app_type,
                                            char ** northbound_listing,
                                            int n_northbound_listing)
{
  rwdynschema_dynamic_schema_registration_t *boxed =
      (rwdynschema_dynamic_schema_registration_t *)g_new0(rwdynschema_dynamic_schema_registration_t, 1);

  boxed->ref_count = 1;
    
  boxed->dts_handle = dts_handle;
  boxed->app_instance = app_instance;
  boxed->app_sub_cb = app_sub_cb;
  
  boxed->batch_size = 0;
  boxed->batch_capacity = 128;

  boxed->modules = RW_MALLOC(boxed->batch_capacity * sizeof(rwdynschema_module_t));

  boxed->app_type = app_type;
  boxed->n_northbound_listing = n_northbound_listing;
  boxed->northbound_listing = RW_MALLOC(n_northbound_listing * sizeof(char*));

  int i = 0;
  for (i = 0; i < n_northbound_listing; ++i) {
    boxed->northbound_listing[i] = RW_STRDUP(northbound_listing[i]);
  }

  boxed->app_name = RW_STRDUP(app_name);
  boxed->memlog_instance = rwmemlog_instance_alloc(app_name, 0, 200);
  boxed->memlog_buffer = rwmemlog_instance_get_buffer(boxed->memlog_instance,
                                                      "dynamic_schema",
                                                      (intptr_t)(app_instance));

  return boxed;
}

void rwdynschema_grow_module_array(rwdynschema_dynamic_schema_registration_t * reg)
{
  reg->batch_capacity = reg->batch_capacity * 2;
  reg->modules = RW_REALLOC(reg->modules, reg->batch_capacity * sizeof(rwdynschema_module_t));  
}

void rwdynschema_add_module_to_batch(rwdynschema_dynamic_schema_registration_t * reg,
                                     const rwdynschema_module_t* module_info)
{
  if (reg->batch_size >= reg->batch_capacity) {
    rwdynschema_grow_module_array(reg);
  }

  reg->modules[reg->batch_size].module_name   = RW_STRDUP(module_info->module_name);
  reg->modules[reg->batch_size].fxs_filename  = RW_STRDUP(module_info->fxs_filename);
  reg->modules[reg->batch_size].so_filename   = RW_STRDUP(module_info->so_filename);
  reg->modules[reg->batch_size].yang_filename = RW_STRDUP(module_info->yang_filename);
  reg->modules[reg->batch_size].metainfo_filename = RW_STRDUP(module_info->metainfo_filename);
  reg->modules[reg->batch_size].exported = module_info->exported;

  ++reg->batch_size;
}

rwdts_member_rsp_code_t rwdynschema_app_commit(rwdts_xact_info_t const * xact_info,
                                               uint32_t n_crec,
                                               rwdts_commit_record_t const ** crec)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdynschema_dynamic_schema_registration_t * app_data =
      (rwdynschema_dynamic_schema_registration_t *) xact_info->ud;
  RW_ASSERT(app_data);

  rw_run_file_update_protocol(xact_info->apih->sched,
                              xact_info->apih->rwtasklet_info,
                              xact_info->apih->tasklet,
                              app_data);
  
  return RWDTS_ACTION_OK;
}

rwdts_member_rsp_code_t
rwdynschema_app_prepare(rwdts_xact_info_t const * xact_info,
                        RWDtsQueryAction action,
                        rw_keyspec_path_t const * keyspec,
                        ProtobufCMessage const * msg,
                        uint32_t credits,
                        void * get_next_key)
{
  if (action == RWDTS_QUERY_UPDATE) {
    return RWDTS_ACTION_OK;
  }

  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdynschema_dynamic_schema_registration_t * app_data =
      (rwdynschema_dynamic_schema_registration_t *) xact_info->ud;
  RW_ASSERT(app_data);

  RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules) * dynamic_modules
      = (RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules) *) msg;

  rwdynschema_module_t mod_info;
  mod_info.module_name   = dynamic_modules->name;
  mod_info.fxs_filename  = dynamic_modules->module->fxs_filename;
  mod_info.so_filename   = dynamic_modules->module->so_filename;
  mod_info.yang_filename = dynamic_modules->module->yang_filename;
  mod_info.metainfo_filename = dynamic_modules->module->metainfo_filename;
  mod_info.exported = dynamic_modules->module->exported;

  rwdynschema_add_module_to_batch(app_data, &mod_info);
  
  return RWDTS_ACTION_OK;
}

rwdynschema_dynamic_schema_registration_t *
rwdynschema_instance_register(rwdts_api_t * dts_handle,
                              rwdynschema_app_sub_cb app_sub_cb,
                              char const * app_name,
                              RwdynschemaAppType app_type,
                              void * app_instance,
                              GDestroyNotify app_instance_destructor)
{

  char **northbound_listing = NULL;
  int n_northbound_listing = 0;

  northbound_listing = extract_northbound_listing_from_manifest(dts_handle, &n_northbound_listing);
  RW_ASSERT (northbound_listing);

  // construct app data collection 
  rwdynschema_dynamic_schema_registration_t * app_data =
      rwdynschema_dynamic_schema_registration_new(app_name,
                                                  dts_handle,
                                                  app_instance,
                                                  app_sub_cb,
                                                  app_type,
                                                  northbound_listing,
                                                  n_northbound_listing);

  rw_yang_pb_schema_t const * mgmt_schema = rw_load_schema("librwschema_yang_gen.so", "rw-mgmt-schema");
  rwdts_api_add_ypbc_schema(dts_handle, mgmt_schema);

  // register with DTS
  RWPB_T_PATHSPEC(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules) sub_keyspec_entry =
      (*RWPB_G_PATHSPEC_VALUE(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules));

  rw_keyspec_path_t *sub_keyspec = (rw_keyspec_path_t*)&sub_keyspec_entry;
  rw_keyspec_path_set_category((rw_keyspec_path_t*)sub_keyspec,
                               NULL ,
                               RW_SCHEMA_CATEGORY_DATA);
                               
  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);

  reg_cb.cb.prepare = rwdynschema_app_prepare;
  reg_cb.cb.commit = rwdynschema_app_commit;
  reg_cb.ud = (void *)app_data;

  rwdts_member_reg_handle_t sub_registration = rwdts_member_register(NULL,
                                                                     dts_handle,
                                                                     sub_keyspec,
                                                                     &reg_cb,
                                                                     RWPB_G_MSG_PBCMD(
                                                                         RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules),
                                                                     RWDTS_FLAG_SUBSCRIBER,
                                                                     NULL);
  
  // save DTS details for app
  app_data->dts_registration = sub_registration;

  // Create the runtime schema directories on this vm, if not already created, and load the schema

  rwsched_dispatch_queue_t queue = rwsched_dispatch_queue_create(
      app_data->dts_handle->tasklet,
      "dynamic schema queue",
      RWSCHED_DISPATCH_QUEUE_SERIAL);

  app_data->load_queue = queue;

  rwsched_dispatch_async_f(app_data->dts_handle->tasklet,
                           queue,
                           (void *)app_data,
                           rwdynschema_load_all_schema);

  return app_data;
}

