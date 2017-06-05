/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Creation Date: 9/17/15
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#ifndef __RWDYNSCHEMA_H__
#define __RWDYNSCHEMA_H__

#include <glib.h>
#include <glib-object.h>

#include "rwdts.h"
#include "rwvcs.h"
#include "rwmemlog.h"
#include "yangmodel.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum _RwdynschemaAppType {
    RWDYNSCHEMA_APP_TYPE_NORTHBOUND = 0,
    RWDYNSCHEMA_APP_TYPE_OTHER = 1,
    RWDYNSCHEMA_APP_TYPE_AGENT = 2
  } RwdynschemaAppType;

  struct rwdynschema_module_s {
    char * module_name;
    char * fxs_filename;
    char * so_filename;
    char * yang_filename;
    char * metainfo_filename;
    size_t exported;    
  };
  typedef struct rwdynschema_module_s rwdynschema_module_t;

  /*!
   * 
   */
  /// @cond GI_SCANNER
  /**
   * rwdynschema_app_sub_cb:
   * @app_instance: (type gpointer)
   * @numel:  
   * @modules: (transfer none) (array length=numel) (allow-none):
   */
  /// @endcond
  typedef void (*rwdynschema_app_sub_cb)(
      void * app_instance,
      int const numel,
      rwdynschema_module_t * modules);

  struct rwdynschema_dynamic_schema_registration_s {
#ifndef __GI_SCANNER__
    gint ref_count;

    rwdts_api_t * dts_handle;
    rwdts_member_reg_handle_t dts_registration;
    void * app_instance;
    rwdynschema_app_sub_cb app_sub_cb;
    char * app_name;

    int batch_size;
    int batch_capacity;
    rwdynschema_module_t * modules;

    RwdynschemaAppType app_type;
    char ** northbound_listing;
    int n_northbound_listing;

    rwsched_dispatch_queue_t load_queue;

    rwmemlog_instance_t * memlog_instance;
    rwmemlog_buffer_t * memlog_buffer;
#endif //__GI_SCANNER__
  };
  typedef struct rwdynschema_dynamic_schema_registration_s rwdynschema_dynamic_schema_registration_t;


#ifndef __GI_SCANNER__
  rwdynschema_dynamic_schema_registration_t *
  rwdynschema_dynamic_schema_registration_new(char const * app_name,
                                              rwdts_api_t * dts_handle,
                                              void * app_instance,
                                              rwdynschema_app_sub_cb app_sub_cb,
                                              RwdynschemaAppType app_type,
                                              char ** northbound_listing,
                                              int n_northbound_listing);
#endif // __GI_SCANNER__

  GType rwdynschema_dynamic_schema_registration_get_type(void);

  /*!
   * Register an app to get updates about dynamic schema changes.
   */
  /// @cond GI_SCANNER
  /**
   * rwdynschema_instance_register:
   * @dts_handle: (type RwDts.Api)
   * @app_sub_cb: (scope notified) (destroy app_instance_destructor) (closure app_instance)
   * @app_name:
   * @app_type:
   * @app_instance: (type gpointer)
   * @app_instance_destructor: (scope async) (nullable)
   * Returns: (transfer none)
   */
  /// @endcond
  rwdynschema_dynamic_schema_registration_t *
  rwdynschema_instance_register(rwdts_api_t * dts_handle,
                                rwdynschema_app_sub_cb app_sub_cb,
                                char const * app_name,
                                RwdynschemaAppType app_type,
                                void * app_instance,
                                GDestroyNotify app_instance_destructor);

#ifndef __GI_SCANNER__

  void rw_run_file_update_protocol(rwsched_instance_ptr_t sched,
                                   rwtasklet_info_ptr_t tinfo,
                                   rwsched_tasklet_ptr_t tasklet,
                                   rwdynschema_dynamic_schema_registration_t* app_data);

  bool rw_create_runtime_schema_dir(char const *const * northbound_schema_listing,
                                    int n_northbound_listing);

  void rwdynschema_add_module_to_batch(rwdynschema_dynamic_schema_registration_t * reg,
                                       const rwdynschema_module_t* module_info);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
