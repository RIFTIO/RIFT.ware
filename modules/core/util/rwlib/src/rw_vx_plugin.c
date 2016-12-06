
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
 */


/**
 * @file rw_vx_plugin.c
 * @author Matt Harper (matt.harper@riftio.com)
 * @date 07/24/2013
 * @brief RiftWare Virtual Executive plug/port/pin framework
 * 
 * @details RiftWare(RW) (V)irtual E(X)ecutive (RW_VX) engine to manipulate RiftWare plugins
 */
#include "rw_vx_plugin.h"

#if !defined(INSTALLDIR)
/* Need to Know where PEAS is installed so that is can get the loaders */
#error - Makefile must define INSTALLDIR
#endif /* !defined(INSTALLDIR) */


/*
 * Utility function which returns the GObject which provides the interfaces
 */
static GObject *
rw_vx_modinst_iface_provider(rw_vx_modinst_common_t *mip)
{
  GObject *result = NULL;
  RW_ASSERT(RW_VX_MODINST_VALID(mip));
  RW_ASSERT(RW_VX_MODULE_VALID(mip->modp));

  if (mip->modp->peas_extension) {
    result = mip->modp->peas_extension;
  }
  else {
    RW_ASSERT(mip->modinst_handle);
    result = mip->modinst_handle;
  }
  return result;
}



/*
 * Release all allocated plugs
 *
 * NOTE: since link handles contain plugs this should be called
 *       rw_vx_link_handle_free_all(rw_vxfp) has already been called
 */
static void
rw_vx_plug_free_all(rw_vx_framework_t *rw_vxfp)
{
  rw_vx_plug_t *plug;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  RW_ASSERT(0 == RW_DL_LENGTH(&rw_vxfp->link_handle_list));

  while(1) {
    plug = RW_DL_HEAD(&rw_vxfp->plug_list,rw_vx_plug_t,dl_elem);
    if (NULL == plug) {
      break;
    }
    rw_vx_plug_free(plug);
  }
}



/*
 * Release all link handles
 */
static void
rw_vx_link_handle_free_all(rw_vx_framework_t *rw_vxfp)
{
  rw_vx_link_handle_t *link_handle;
  rw_status_t rw_status;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  UNUSED(rw_status);

  while(1) {
    link_handle = RW_DL_HEAD(&rw_vxfp->link_handle_list,rw_vx_link_handle_t,dl_elem);
    if (NULL == link_handle) {
      break;
    }

    /* A link handle is released by calling rw_vx_unlink() on it */
    rw_status = rw_vx_unlink(link_handle);
    RW_ASSERT(RW_STATUS_SUCCESS == rw_status);
  }
}



/*
 * This callback is installed via g_log_set_default_handler()
 * to intercept all g_log() calls
 */
static void
rw_vx_g_log_handler(const gchar    *log_domain,
                      GLogLevelFlags  log_level,
                      const gchar    *message,
                      gpointer        user_data)
{
  //#define RW_VX_SUPPRESS_EXPECTED_LOGS
#if defined(RW_VX_SUPPRESS_EXPECTED_LOGS)
  static const gchar *rw_vx_g_log_allowed_patterns[] = {
    "*lib*loader.so*cannot open shared object file: No such file or directory*"
  };
  guint i;
  for (i = 0; i < G_N_ELEMENTS (rw_vx_g_log_allowed_patterns); ++i) {
    if (g_pattern_match_simple (rw_vx_g_log_allowed_patterns[i], message))
      return;
  }
#endif /* defined(RW_VX_SUPPRESS_EXPECTED_LOGS) */
  g_log_default_handler(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, message, user_data);
}


static rw_status_t
rw_vx_setup_repository_from_environment(rw_vx_framework_t *vxfp)
{
  char *typelib_path = getenv("GI_TYPELIB_PATH");
  if (typelib_path) {
    char *temp_path = strdup(typelib_path);
    char *saveptr, *token;
    token = strtok_r(temp_path, ":", &saveptr);
    while (token != NULL) {
      if (token) {
        rw_vx_prepend_gi_search_path(token);
      }
      token = strtok_r(NULL, ":", &saveptr);
    }
    free(temp_path);
  }

  char *plugindir = getenv("PLUGINDIR");
  if (plugindir) {
    char *temp_path = strdup(plugindir);
    char *saveptr, *token;
    token = strtok_r(temp_path, ":", &saveptr);
    while (token != NULL) {
      if (token) {
        rw_vx_add_peas_search_path(vxfp, token);
      }
      token = strtok_r(NULL, ":", &saveptr);
    }
    free(temp_path);
  }

  return RW_STATUS_SUCCESS;
}


/*
 * Initialize LIBPEAS for loading Modules
 */
static rw_status_t
rw_vx_modules_setup_libpeas(rw_vx_framework_t *rw_vxfp)
{
  // gchar progname[] = "rw_vx-framework";
  gchar progname[] = "";
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));


  /*
   * Initializ GLib & GNOME
   */
#if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
#endif
  g_set_prgname(progname);

  /* Don't abort on warnings or criticals*/
  g_log_set_always_fatal(G_LOG_LEVEL_ERROR);
  g_log_set_default_handler(rw_vx_g_log_handler, NULL);
  g_setenv("GSETTINGS_BACKEND", "memory", TRUE);


  /*
   * Initialize LIBPEAS
   */

  /* Setup the repository paths */
  rw_vx_prepend_gi_search_path("/usr/lib64/girepository-1.0");
  rw_vx_require_repository("Peas", "1.0");

  /*
   * Set the PEAS environement variables
   * Prevent python from generating compiled files, they break distcheck
   */
  // Use the environment set by rift-shell
  //g_setenv("PEAS_PLUGIN_LOADERS_DIR", "/usr/lib64/libpeas-1.0/loaders", TRUE);
  g_setenv("PYTHONDONTWRITEBYTECODE", "yes", TRUE);

  
  /*
   * Initialize a PEAS engine
   * Allocate an engine with either gloabl or nonglobal loader
   */
  if (rw_vxfp->loader_type == RW_VX_GLOBAL_LOADERS) {
    rw_vxfp->engine = peas_engine_get_default();
    // rw_vxfp->engine = peas_engine_new();
  } else if (rw_vxfp->loader_type == RW_VX_NONGLOBAL_LOADERS) {
    rw_vxfp->engine = peas_engine_new_with_nonglobal_loaders();
  } else {
    RW_CRASH_MESSAGE("Invalid loader type %d, "
                     "expecting either RW_VX_GLOBAL_LOADERS[%d] or "
                     "RW_VX_NONGLOBAL_LOADERS[%d]\n", 
                     rw_vxfp->loader_type, RW_VX_GLOBAL_LOADERS, RW_VX_NONGLOBAL_LOADERS);
  }
  RW_ASSERT(rw_vxfp->engine);

  /*
   * If the PEAS GObject engine is free'd, rw_vxfp->engine will be set to NULL automagically
   */
  g_object_add_weak_pointer(G_OBJECT(rw_vxfp->engine), (gpointer *) &rw_vxfp->engine);

  /*
   * Enable every possible language loader
   */
  peas_engine_enable_loader(rw_vxfp->engine, "python3");
  peas_engine_enable_loader(rw_vxfp->engine, "lua5.1");

  rw_vx_setup_repository_from_environment(rw_vxfp);

  return RW_STATUS_SUCCESS;
}



/*
 * Helper routine called to alloc/init the storage for a module instance
 * associated with a module
 */
static rw_vx_modinst_common_t *
rw_vx_modinst_common_init(rw_vx_module_common_t *modp,
                            uint32_t minor,
                            char *alloc_cfg)
{
  rw_status_t rw_status;
  rw_vx_modinst_common_t *mip;
  RW_ASSERT(modp);
  RW_ASSERT(alloc_cfg);
  UNUSED(rw_status);

  mip = RW_MALLOC0(sizeof(*mip));

  mip->rw_vxfp = modp->rw_vxfp;   /* save RW_VX framework in the modinst       */
  mip->modp = modp;                   /* Remember module for this modinst       */
  mip->minor = minor;                 /* Save minor modinst_id for this modinst */
  mip->magic = RW_VX_MODINST_MAGIC;

  mip->alloc_cfg = RW_STRDUP(alloc_cfg);
  RW_ASSERT(mip->alloc_cfg);

  /*
   * Insert into the modinst array at the proper location
   */
  rw_status = RW_SKLIST_INSERT(&modp->all_modinsts_sklist,mip);
  RW_ASSERT(RW_STATUS_SUCCESS == rw_status);

  return mip;
}



/*
 * Helper routine called to release a module instance
 * associated with a module
 */
static void
rw_vx_modinst_common_deinit(rw_vx_modinst_common_t *mip)
{
  rw_vx_modinst_common_t *dp = NULL;
  RW_ASSERT(RW_VX_MODINST_VALID(mip));
  RW_ASSERT(RW_VX_MODULE_VALID(mip->modp));
  
  /* Remove the modinst from the module's sklist */   
  RW_SKLIST_REMOVE_BY_KEY(&mip->modp->all_modinsts_sklist,
                            &mip->minor,&dp);
  RW_ASSERT(mip == dp);

  if (mip->alloc_cfg) {
    RW_FREE(mip->alloc_cfg);
    mip->alloc_cfg = NULL;
  }
  
  mip->magic = RW_VX_BAD_MAGIC;
  RW_FREE(mip);
}



/*
 * Un-install the PEAS plugin installed as "module_name"
 */
rw_status_t
rw_vx_module_unregister(rw_vx_framework_t *rw_vxfp,
                          char *module_name)
{
  rw_status_t rw_status;
  rw_vx_module_common_t *modp = NULL, *tmp=NULL;
  rw_vx_modinst_common_t *p;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  RW_ASSERT(module_name);

  /*
   * Lookup the provided module name to verify module has been registered
   */
  rw_status = RW_SKLIST_LOOKUP_BY_KEY(&rw_vxfp->module_sklist,&module_name,&tmp);
  if (tmp) {
    rw_status = RW_SKLIST_REMOVE_BY_KEY(&rw_vxfp->module_sklist,&module_name,&modp);
    RW_ASSERT(RW_STATUS_SUCCESS == rw_status);
    RW_ASSERT(tmp == modp);

    /*
     * Walk all valid module instances and close each one
     */
    for(p = RW_SKLIST_HEAD(&modp->all_modinsts_sklist,rw_vx_modinst_common_t);
        p;
        p = RW_SKLIST_HEAD(&modp->all_modinsts_sklist,rw_vx_modinst_common_t)) {
      rw_status = rw_vx_modinst_close(p);
      RW_ASSERT(RW_STATUS_SUCCESS == rw_status);
    }

    if (modp->peas_extension) {
      RW_ASSERT(PEAS_IS_EXTENSION(modp->peas_extension));
      if (modp->ci_interface) {
        RW_ASSERT(modp->ci_interface);
        RW_ASSERT((modp->ci_interface == RWPLUGIN_API_GET_INTERFACE(modp->peas_extension)));
        RW_ASSERT(modp->ci_api);
        RW_ASSERT(RWPLUGIN_IS_API(modp->ci_api));
      }
    }
    
    /*
     * If this module supports the COMMON_INTERFACE, then
     * call the COMMON_INTERFACE module-specific de-init function
     */
    if (modp->ci_interface) {
      RW_ASSERT(modp->ci_api);
      if (modp->ci_interface->module_deinit) {
        RW_ASSERT(modp->module_handle);
        modp->ci_interface->module_deinit(modp->ci_api,modp->module_handle);
      }
      modp->module_handle = NULL;
      modp->ci_interface = NULL;
      modp->ci_api = NULL;
    }
    
    /* Free sklist of all minor module instances */
    RW_ASSERT(0 == RW_SKLIST_LENGTH(&modp->all_modinsts_sklist));
    RW_SKLIST_FREE(&modp->all_modinsts_sklist);  
    
    /*
     * The associated PEAS extension needs to be destroyed
     */
    if (modp->peas_extension) {
      RW_ASSERT(PEAS_IS_EXTENSION(modp->peas_extension));
      g_object_unref(modp->peas_extension);
      modp->peas_extension = NULL;
    }

    /* Free the memory allocated for the module */
    RW_ASSERT(modp->module_name);
    RW_FREE(modp->module_name);
    modp->module_name = NULL;
    RW_ASSERT(modp->module_alloc_cfg);
    RW_FREE(modp->module_alloc_cfg);
    modp->module_alloc_cfg = NULL;    

    modp->magic = RW_VX_BAD_MAGIC;
    RW_FREE(modp);

    rw_status = RW_STATUS_SUCCESS;
  }

  return rw_status;
}



/*
 * Install the module to support pseudo-module instances
 */
static void
rw_vx_pseudo_module_register(rw_vx_framework_t *rw_vxfp)
{
  rw_status_t rw_status;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  char rw_vx_pseudo_module_name[] = RW_VX_PSEUDO_MODULE_NAME;
  char null_string[] = "";

  rw_status = rw_vx_module_register(rw_vxfp,
                                        rw_vx_pseudo_module_name,
                                        null_string, /* module alloc config */
                                        null_string, /* plugin_name */
                                        NULL);
  RW_ASSERT(RW_STATUS_SUCCESS == rw_status);
}


/*
 * UN-Install the module to support pseudo-module instances
 */
static void
rw_vx_pseudo_module_unregister(rw_vx_framework_t *rw_vxfp)
{
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));

  rw_status_t rw_status;
  char rw_vx_pseudo_module_name[] = RW_VX_PSEUDO_MODULE_NAME;
  
  rw_status = rw_vx_module_unregister(rw_vxfp,
                                          rw_vx_pseudo_module_name);
  RW_ASSERT(RW_STATUS_SUCCESS == rw_status);
}

rw_vx_framework_t *
rw_vx_framework_alloc_internal(rw_vx_loader_type_t type)
{
  rw_status_t rw_status;
  rw_vx_framework_t *rw_vxfp;
  UNUSED(rw_status);

  rw_vxfp = (rw_vx_framework_t *)RW_MALLOC0(sizeof(*rw_vxfp));
  rw_vxfp->magic = RW_VX_FRAMEWORK_MAGIC;
  rw_vxfp->loader_type = type;

  RW_SKLIST_PARAMS_DECL(tmp,rw_vx_module_common_t,module_name,rw_sklist_comp_charptr,module_sklist_element);
  RW_SKLIST_INIT(&rw_vxfp->module_sklist,&tmp);

  /*
   * Initializes a LIBPEAS instance 
   * and set the search path for StandardPlugin which will be used in subsequent tests
   */
  rw_status = rw_vx_modules_setup_libpeas(rw_vxfp);
  RW_ASSERT(RW_STATUS_SUCCESS == rw_status);

  rw_vx_pseudo_module_register(rw_vxfp);
  
  return rw_vxfp;
}

/*
 * Allocate RW_VX framework
 */
rw_vx_framework_t *
rw_vx_framework_alloc_nonglobal()
{
  return rw_vx_framework_alloc_internal(RW_VX_NONGLOBAL_LOADERS);
}

/*
 * Allocate RW_VX framework
 */
rw_vx_framework_t *
rw_vx_framework_alloc()
{
  return rw_vx_framework_alloc_internal(RW_VX_GLOBAL_LOADERS);
}

/*
 * Deallocate a RW_VX framework
 */
rw_status_t
rw_vx_framework_free(rw_vx_framework_t *rw_vxfp)
{
  rw_vx_module_common_t *modp;
  rw_status_t rw_status;
  rw_status_t retval = RW_STATUS_SUCCESS;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));

  /* Release any open link handles */
  rw_vx_link_handle_free_all(rw_vxfp);
  RW_ASSERT(0 == RW_DL_LENGTH(&rw_vxfp->link_handle_list));
  
  /* Release any plugs still allocated */
  rw_vx_plug_free_all(rw_vxfp);
  RW_ASSERT(0 == RW_DL_LENGTH(&rw_vxfp->plug_list));

  rw_vx_pseudo_module_unregister(rw_vxfp);
  
  /* Unregister all modules */
  while(1) {
    modp = RW_SKLIST_HEAD(&rw_vxfp->module_sklist,rw_vx_module_common_t);
    if (NULL == modp) {
      break;
    }
    RW_ASSERT(modp->rw_vxfp == rw_vxfp);
    rw_status = rw_vx_module_unregister(rw_vxfp,modp->module_name);
    RW_ASSERT(RW_STATUS_SUCCESS == rw_status);
    if (rw_status != RW_STATUS_SUCCESS) {
      retval = rw_status;
    }
    RW_ASSERT(modp != RW_SKLIST_HEAD(&rw_vxfp->module_sklist,rw_vx_module_common_t));
  }

  /* Free the module skiplist itself */
  rw_status = RW_SKLIST_FREE(&rw_vxfp->module_sklist);
  RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
  
  /* Free the RW_VX framework */
  RW_FREE(rw_vxfp);

  return retval;
}



/*
 * Perform actual install a module based on PEAS extension
 */
static rw_status_t
rw_vx_try_add_plugin_module(rw_vx_framework_t *rw_vxfp,
                              char *module_name,
                              PeasPluginInfo *plugin_info,
                              char *module_alloc_cfg,
                              rw_vx_module_common_t **modpp)
{
  rw_status_t rw_status;
  rw_vx_module_common_t *modp = NULL, *tmp = NULL;
  PeasExtension *peas_extension = NULL;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  RW_ASSERT(module_name);
  RW_ASSERT(module_alloc_cfg);
  RW_ASSERT(modpp);

  /*
   * Ensure no duplicate module names
   */
  rw_status = RW_SKLIST_LOOKUP_BY_KEY(&rw_vxfp->module_sklist,&module_name,&tmp);
  if (NULL != tmp) {
    goto done;
  }


  /*
   * Allocate memory for the module initialize the common part
   */
  modp = (rw_vx_module_common_t *) RW_MALLOC0(sizeof(*modp));
  modp->rw_vxfp = rw_vxfp;
  modp->plugin_info = plugin_info;
  modp->module_name = RW_STRDUP(module_name);
  modp->module_alloc_cfg = RW_STRDUP(module_alloc_cfg);
  modp->magic = RW_VX_MODULE_MAGIC;


  /*
   * Determine if this PEAS module supports the COMMON INTERFACE
   * If yes, create the extension and then get the api and interface pointers for the COMMON_INTERFACE API
   */
  if (plugin_info) {
    if (peas_engine_provides_extension(rw_vxfp->engine, plugin_info, RWPLUGIN_TYPE_API)) {
      /* Create an instance of the extension */
      peas_extension = peas_engine_create_extension(rw_vxfp->engine, plugin_info,
                                                    RWPLUGIN_TYPE_API,
                                                    NULL);
      if (NULL == peas_extension) {
        rw_status = RW_STATUS_FAILURE;
        goto done;
      }
      RW_ASSERT(PEAS_IS_EXTENSION(peas_extension));
      modp->peas_extension = peas_extension;
      modp->ci_api = RWPLUGIN_API(peas_extension);
      modp->ci_interface = RWPLUGIN_API_GET_INTERFACE(peas_extension);
      RW_ASSERT(modp->ci_api);
      RW_ASSERT(modp->ci_interface);
    }
    else {
      /*
       * This is a simple PEAS module which doesn't support the RWPLUGIN_TYPE_API
       * The extension is NOT created here since the type of the API to be used is not yet known
       */
    }
  }
  else {
    /* This is a pseudo-module */
    RW_ASSERT(!strcmp(module_name,RW_VX_PSEUDO_MODULE_NAME));
  }

  /* Allocate sklist to hold all module instances */
  RW_SKLIST_PARAMS_DECL(_tmp,rw_vx_modinst_common_t,minor,rw_sklist_comp_uint32_t,all_modinsts_sklist_elem);
  RW_SKLIST_INIT(&modp->all_modinsts_sklist,&_tmp);

  
  /*
   * Call the module-specific init function
   */
  rw_status = RW_STATUS_SUCCESS;
  if (modp->ci_interface) {
    RW_ASSERT(modp->ci_interface->module_init);
    modp->module_handle = modp->ci_interface->module_init(modp->ci_api,
                                                          modp->module_name,
                                                          modp->module_alloc_cfg);

    if (NULL == modp->module_handle) {
      rw_status = RW_STATUS_FAILURE;
      goto done;
    }
  }

  rw_status = RW_SKLIST_INSERT(&rw_vxfp->module_sklist,modp);
  RW_ASSERT(RW_STATUS_SUCCESS == rw_status);

done:

  if (rw_status != RW_STATUS_SUCCESS) {
    if (NULL != modp) {
      /* Free any memory allocated for the module */
      if (modp->module_name) {
        RW_FREE(modp->module_name);
        modp->module_name = NULL;
      }
      if (modp->module_alloc_cfg) {
        RW_FREE(modp->module_alloc_cfg);
        modp->module_alloc_cfg = NULL;
      }
      if (modp->peas_extension) {
        RW_ASSERT(PEAS_IS_EXTENSION(modp->peas_extension));
        g_object_unref(modp->peas_extension);
        modp->peas_extension = NULL;
      }
      modp->magic = RW_VX_BAD_MAGIC;
      RW_FREE(modp);
      modp = NULL;
    }
  }
  
  if (modpp) {
    *modpp = modp;
  }

  return rw_status;
}



/*
 * Register a module type with a RW_VX framework
 *
 * NOTE:
 * param "extension_type" is the GType of <any> interface supported by the plugin
 * If the common interface is supported, then pass RWPLUGIN_TYPE_API can be used
 */
rw_status_t
rw_vx_module_register(rw_vx_framework_t *rw_vxfp,
                        char *module_name,      // Name by which system will refer to this module
                        char *module_alloc_cfg, // Any options required when registering/initing the module
                        char *extension_name,   // FileName of plugin to load (e.g. "standard_plugin-c", "standard_plugin-python.python")
                        rw_vx_module_common_t **modpp) // Optional pointer to pass back the new module instance ptr
{
  rw_status_t rw_status;
  rw_vx_module_common_t *modp = NULL, *tmp = NULL;
  PeasPluginInfo *plugin_info = NULL;
  gchar *error_msg = NULL;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  RW_ASSERT(module_name);
  RW_ASSERT(module_alloc_cfg);
  RW_ASSERT(extension_name);
  
  /*
   * Ensure module_name doesn't already exist
   */
  rw_status = RW_SKLIST_LOOKUP_BY_KEY(&rw_vxfp->module_sklist,&module_name,&tmp);
  if (NULL != tmp) {
    error_msg = g_strdup_printf("module name '%s' has already been registered",module_name);
    rw_status = RW_STATUS_FAILURE;
    goto done;
  }

  /*
   * If not the pseudo-module, have the PEAS engine load it
   */
  if (!strcmp(module_name,RW_VX_PSEUDO_MODULE_NAME)) {
    RW_ASSERT(!module_alloc_cfg[0]);
    RW_ASSERT(!extension_name[0]);
  }
  else {
    /* It is a PEAS module */
    plugin_info = peas_engine_get_plugin_info(rw_vxfp->engine, extension_name);
    if (NULL == plugin_info) {
      error_msg = g_strdup_printf("peas_engine_get_plugin_info() failed for extension '%s'",extension_name);
      rw_status = RW_STATUS_FAILURE;
      goto done;
    }
    
    /* Load the code for the extension */
    if (TRUE != peas_engine_load_plugin(rw_vxfp->engine, plugin_info)) {
      error_msg = g_strdup_printf("peas_engine_load_plugin() failed for extension '%s'",extension_name);
      rw_status = RW_STATUS_FAILURE;
      goto done;
    }
  }
  
  rw_status = rw_vx_try_add_plugin_module(rw_vxfp,
                                              module_name,
                                              plugin_info,
                                              module_alloc_cfg,
                                              &modp);
  RW_ASSERT(RW_STATUS_SUCCESS == rw_status);
  RW_ASSERT(NULL != modp);
  RW_ASSERT(modp->rw_vxfp == rw_vxfp);

done:
  if (modpp) {
    *modpp = modp;
  }

  if (RW_STATUS_SUCCESS != rw_status) {
    printf("ERROR: %s\n",error_msg?error_msg:"");
  }

  if (error_msg) {
    g_free(error_msg);
    error_msg = NULL;
  }

  return rw_status;
}



/*
 * Return a pointer to an installed module by module-name
 */
rw_vx_module_common_t *
rw_vx_module_lookup(rw_vx_framework_t *rw_vxfp,
                      char *module_name)
{
  rw_vx_module_common_t *modp = NULL;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  RW_ASSERT(module_name);

  RW_SKLIST_LOOKUP_BY_KEY(&rw_vxfp->module_sklist,&module_name,&modp);
  RW_ASSERT((NULL == modp) || RW_VX_MODULE_VALID(modp));

  return modp;
}



/*
 * Determine exact minor modinst # to open for a modinst and if it is possible
 */
static rw_status_t
rw_vx_modinst_common_select_minor_to_open(rw_vx_module_common_t *modp,
                                            int32_t *minorp)
{
  rw_status_t rw_status = RW_STATUS_SUCCESS;
  rw_vx_modinst_common_t *p;
  RW_ASSERT(RW_VX_MODULE_VALID(modp));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(modp->rw_vxfp));
  RW_ASSERT(minorp);

  if (*minorp <= RW_VX_CLONE_MINOR_MODINST) {
    /*
     * The request is to open the first available modinst
     *
     * Search the modinst list for the first unused minor #
     */
    *minorp = 0;
    for(p = RW_SKLIST_HEAD(&modp->all_modinsts_sklist,rw_vx_modinst_common_t);
        p;
        p = RW_SKLIST_NEXT(p,rw_vx_modinst_common_t,all_modinsts_sklist_elem)) {
      if (p->minor != (uint32_t) *minorp) {
        /* Found an unused minor modinst # */
        break;
      }
      *minorp += 1; // Try the next minor #
    }
  }
  else {
    /* See if the specific minor modinst is already in use */
    RW_SKLIST_LOOKUP_BY_KEY(&modp->all_modinsts_sklist,minorp,&p);
    if (p) {
      /* Already open - fail */
      rw_status = RW_STATUS_FAILURE;
    }
  }
  
  return rw_status;
}



/*
 * This routine returns a pointer to the specified minor modinst instance of a module
 * if it is currently open
 */
rw_vx_modinst_common_t *
rw_vx_modinst_lookup(rw_vx_framework_t *rw_vxfp,
                       char *module_name,
                       uint32_t minor)
{
  rw_vx_module_common_t *modp = NULL;
  rw_vx_modinst_common_t *modinstp = NULL;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));

  RW_SKLIST_LOOKUP_BY_KEY(&rw_vxfp->module_sklist,&module_name,&modp);

  if (modp) {
    RW_ASSERT(RW_VX_MODULE_VALID(modp));

    RW_SKLIST_LOOKUP_BY_KEY(&modp->all_modinsts_sklist,&minor,&modinstp);
    RW_ASSERT((NULL == modinstp) || RW_VX_MODINST_VALID(modinstp));
  }
  return modinstp;
}



/*
 * Open a modinst on the named module
 * Specific minor modinst can be selected, OR
 * if (minor < 0), then the first available modinst
 * in the module will be used
 */
rw_status_t
rw_vx_modinst_open(rw_vx_framework_t *rw_vxfp,
                     char *module_name,
                     int32_t minor,
                     char *minor_alloc_cfg,
                     rw_vx_modinst_common_t **mipp)
{
  rw_status_t rw_status = RW_STATUS_FAILURE;
  rw_vx_module_common_t *modp = NULL;
  rw_vx_modinst_common_t *mip = NULL;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  RW_ASSERT(module_name);
  RW_ASSERT(minor_alloc_cfg);
  RW_ASSERT(mipp);

  RW_SKLIST_LOOKUP_BY_KEY(&rw_vxfp->module_sklist,&module_name,&modp);

  if (modp) {

    /*
     * Determine if the specified minor modinst is available to open
     * or if any modinst is acceptable, then update "minor" to the required value
     */
    rw_status = rw_vx_modinst_common_select_minor_to_open(modp,&minor);
    if (RW_STATUS_SUCCESS == rw_status) {
      // Requested minor modinst is avail (i.e. not already open)

      /*
       * Allocate a new minor modinst
       */
      mip = rw_vx_modinst_common_init(modp,minor,minor_alloc_cfg);
      RW_ASSERT(mip);

      /*
       * Check if the COMMON INTERFACE that provides module instances is supported
       * and that it provides an API to allocate module instances
       */
      if ((NULL != modp->ci_interface) &&
          (NULL != modp->module_handle) &&
          (NULL != modp->ci_interface->module_instance_alloc)) {
        gchar *minor_name_str = NULL;
        RW_ASSERT(modp->ci_interface->module_instance_alloc);
        RW_ASSERT(modp->ci_interface->module_instance_free);
        RW_ASSERT(modp->module_handle);
        RW_ASSERT(NULL == mip->modinst_handle);
        minor_name_str = g_strdup_printf("%u",mip->minor);
        mip->modinst_handle =
            modp->ci_interface->module_instance_alloc(modp->ci_api,
                                                      modp->module_handle,
                                                      minor_name_str,
                                                      mip->alloc_cfg);
        if (NULL != mip->modinst_handle) {
          rw_status = RW_STATUS_SUCCESS;
        }
        else {
          /* De-allocate the module instance */
          rw_vx_modinst_common_deinit(mip);
          mip = NULL;
          rw_status = RW_STATUS_FAILURE;
        }
        g_free(minor_name_str);
        minor_name_str = NULL;
      }
    }
  }
  
  *mipp = mip; /* on error, this will be NULL */
  return rw_status;
}



/*
 * Close a previously opened modinst handle
 * All previous modinst links on the handle must be unlinked
 * for the close to succeed
 */
rw_status_t
rw_vx_modinst_close(rw_vx_modinst_common_t *mip)
{
  rw_status_t rw_status = RW_STATUS_SUCCESS;
  RW_ASSERT(RW_VX_MODINST_VALID(mip));
  RW_ASSERT(RW_VX_MODULE_VALID(mip->modp));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(mip->rw_vxfp));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(mip->modp->rw_vxfp));
  RW_ASSERT(0 == mip->port_refs); /* >0 if a port still points to this modinst */

  /*
   * Call the module instance close routine (which also free's everything)
   */
  if (mip->modp->ci_interface) {
    RW_ASSERT(mip->modp->ci_interface->module_instance_free);
    RW_ASSERT(mip->modinst_handle);
    mip->modp->ci_interface->module_instance_free(mip->modp->ci_api,
                                                  mip->modinst_handle);
  }

  rw_vx_modinst_common_deinit(mip);

  return rw_status;
}



rw_status_t
rw_vx_library_open(rw_vx_framework_t *rw_vxfp,
                     char *extension_name,
                     char *device_alloc_cfg,
                     rw_vx_modinst_common_t **mipp)
{
  char empty_module_config_string[] = "";
  rw_status_t status;
  rw_vx_modinst_common_t *new_mip;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  RW_ASSERT(extension_name);

  /*
   * Has somebody else already opened this library?
   */
  new_mip = rw_vx_library_lookup(rw_vxfp,extension_name);
  if (new_mip) {
    new_mip->used_as_library_count++;
    status = RW_STATUS_SUCCESS; // Already open
    goto done;
  }

  /*
   * Register the library (as a module)
   */
  status = rw_vx_module_register(rw_vxfp,
                                   extension_name, // re-use the extension name as module name, too
                                   empty_module_config_string,
                                   extension_name,
                                   NULL);
  if (RW_STATUS_SUCCESS != status) {
    goto done;
  }
  
  status = rw_vx_modinst_open(rw_vxfp,
                                extension_name,
                                0,
                                device_alloc_cfg,
                                &new_mip);

  if (RW_STATUS_SUCCESS == status) {
    RW_ASSERT(RW_VX_MODINST_VALID(new_mip));
    new_mip->used_as_library_count = 1;
  }
  else {
    RW_ASSERT(NULL == new_mip);
    rw_vx_module_unregister(rw_vxfp,extension_name);
  }

done:

  if (mipp) {
    *mipp = new_mip;
  }

  return status;
}


rw_status_t
rw_vx_library_close(rw_vx_modinst_common_t *mip,
                      gboolean force_unload)
{
  rw_status_t status = RW_STATUS_FAILURE;
  rw_vx_module_common_t *modp;
  RW_ASSERT(RW_VX_MODINST_VALID(mip));
  RW_ASSERT(mip->used_as_library_count > 0);

  modp = mip->modp;
  RW_ASSERT(RW_VX_MODULE_VALID(modp));

  if (mip->used_as_library_count > 0) {
    if ((mip->used_as_library_count == 1) || (TRUE == force_unload)) {
      rw_status_t status2;
      UNUSED(status2);
      
      // Close the module instance backing the library
      status = rw_vx_modinst_close(mip);
      
      // Close the PEAS module backing the library
      status2 = rw_vx_module_unregister(modp->rw_vxfp,modp->module_name);
      RW_ASSERT(RW_STATUS_SUCCESS == status2);
    }
    else {
      mip->used_as_library_count--; // Remove a reference
      status = RW_STATUS_SUCCESS;
    }
  }

  return status;
}



rw_vx_modinst_common_t *
rw_vx_library_lookup(rw_vx_framework_t *rw_vxfp,
                       char *extension_name)
{
  rw_vx_modinst_common_t *mip;

  // There is only instance 0 of a library
  mip = rw_vx_modinst_lookup(rw_vxfp,extension_name,0);

  // If the module isn't a library, then fail
  if (mip && !mip->used_as_library_count) {
    mip = NULL;
  }

  return mip;
}



/*
 * Deallocate a port object
 */
static void
rw_vx_plug_port_free(rw_vx_plug_port_t *port)
{
  rw_vx_port_pin_t *pin;
  RW_ASSERT(RW_VX_PORT_VALID(port));
  RW_ASSERT(RW_VX_PLUG_VALID(port->plug));
  RW_ASSERT(RW_VX_MODINST_VALID(port->mip));

  /*
   * Free each pin of the port
   */
  while(1) {
    pin = RW_DL_REMOVE_HEAD(&port->pin_list,rw_vx_port_pin_t,dl_elem);
    if (NULL == pin) {
      break;
    }

    /* Remove pin from plug-level specific direction pin list */
    switch(pin->pin_dir) {
      case RWPLUGIN_PIN_DIRECTION_INPUT:
        RW_DL_REMOVE(&port->plug->input_pin_list,pin,ft_dl_elem);
        break;
      case RWPLUGIN_PIN_DIRECTION_OUTPUT:
        RW_DL_REMOVE(&port->plug->output_pin_list,pin,ft_dl_elem);
        break;
      default:
        RW_CRASH();
        break;
    }

    pin->magic = RW_VX_BAD_MAGIC;
    RW_FREE(pin);
  }

  /* remove port from the per-plug list */
  RW_DL_REMOVE(&port->plug->port_list,port,dl_elem);

  port->mip->port_refs--;
  port->magic = RW_VX_BAD_MAGIC;
  RW_FREE(port);
}



/*
 * Allocate a plug object
 */
rw_vx_plug_t *
rw_vx_plug_alloc(rw_vx_framework_t *rw_vxfp)
{
  rw_vx_plug_t *plug;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));

  plug = (rw_vx_plug_t *)RW_MALLOC0(sizeof(*plug));
  plug->magic = RW_VX_PLUG_MAGIC;
  plug->rw_vxfp = rw_vxfp;
  RW_DL_INSERT_TAIL(&rw_vxfp->plug_list,plug,dl_elem);

  return plug;
}



/*
 * Free a plug object (and anything it contains
 */
void
rw_vx_plug_free(rw_vx_plug_t *plug)
{
  rw_vx_plug_port_t *port;
  RW_ASSERT(RW_VX_PLUG_VALID(plug));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(plug->rw_vxfp));

  /*
   * Free each port in the plug (+ their respective pins)
   * before freeing the plug itself
   */
  while(1) {
    port = RW_DL_HEAD(&plug->port_list,rw_vx_plug_port_t,dl_elem);
    if (NULL == port) {
      break;
    }
    rw_vx_plug_port_free(port);
  }

  // Remove the plug from the list
  RW_DL_REMOVE(&plug->rw_vxfp->plug_list,plug,dl_elem);

  plug->magic = RW_VX_BAD_MAGIC;
  RW_FREE(plug);
}



/*
 * Add a port object to a plug using the default blockid
 */
rw_vx_plug_port_t *
rw_vx_plug_add_port(rw_vx_plug_t *plug,
                      rw_vx_modinst_common_t *mip)
{
  return rw_vx_plug_add_port_with_blockid(plug,mip,RW_VX_BLOCKID_DEFAULT);
}



/*
 * Add a port object to a plug using the specified blockid
 */
rw_vx_plug_port_t *
rw_vx_plug_add_port_with_blockid(rw_vx_plug_t *plug,
                                   rw_vx_modinst_common_t *mip,
                                   uint32_t blockid)
{
  rw_vx_plug_port_t *port;
  RW_ASSERT(RW_VX_PLUG_VALID(plug));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(plug->rw_vxfp));
  RW_ASSERT(RW_VX_MODINST_VALID(mip));
  RW_ASSERT(RW_VX_MODULE_VALID(mip->modp));

  port = RW_MALLOC0(sizeof(*port));
  port->magic = RW_VX_PORT_MAGIC;
  port->plug = plug;
  port->mip = mip;
  mip->port_refs++; // Ref count to prevent freeing of a module instance with ports/plugs to it
  port->blockid = blockid;
  RW_DL_INSERT_TAIL(&plug->port_list,port,dl_elem);
  
  return port;
}


/*
 * Using the supplied module instance, get the API handle and interface for the requested API
 */
rw_status_t
rw_vx_modinst_get_api_and_iface(rw_vx_modinst_common_t *mip,
                                  GType api_type,
                                  void **p_api,
                                  void **p_iface,
                                  const gchar *first_property,
                                  ...)
{
  rw_status_t rw_status = RW_STATUS_FAILURE;
  va_list var_args;
  RW_ASSERT(RW_VX_MODINST_VALID(mip));
  RW_ASSERT(RW_VX_MODULE_VALID(mip->modp));
  RW_ASSERT(api_type);
  RW_ASSERT(NULL != p_api);
  RW_ASSERT(NULL != p_iface);

  va_start(var_args,first_property);
  
  *p_api = NULL;
  *p_iface = NULL;

  /*
   * Verify the requested API is supported by the plugin
   */
  if (mip->modp->plugin_info &&
      peas_engine_provides_extension(mip->rw_vxfp->engine,
                                     mip->modp->plugin_info,
                                     api_type)) {
    /*
     * Create extension (if it doesn't already exist)
     * This only occurs when RWPLUGIN_TYPE_API wasn't suppported by the plugin
     * when the plugin was loaded
     */
    if (NULL == mip->modp->peas_extension) {
      mip->modp->peas_extension = peas_engine_create_extension_valist(mip->rw_vxfp->engine,
                                                                      mip->modp->plugin_info,
                                                                      api_type,
                                                                      first_property,
                                                                      var_args);
      if (NULL == mip->modp->peas_extension) {
        rw_status = RW_STATUS_FAILURE;
        goto done;
      }
    }
      
    RW_ASSERT(PEAS_IS_EXTENSION(mip->modp->peas_extension));
    *p_api = (G_TYPE_CHECK_INSTANCE_CAST (rw_vx_modinst_iface_provider(mip), api_type, GObject));
    RW_ASSERT(*p_api);
    *p_iface = (G_TYPE_INSTANCE_GET_INTERFACE (rw_vx_modinst_iface_provider(mip), api_type, GTypeInterface));
    RW_ASSERT(*p_iface);
    rw_status = RW_STATUS_SUCCESS;
  }
  else {
    /* The pseudo-modinst case - no PEAS extension */
    RW_ASSERT(NULL == mip->modp->peas_extension);
    *p_api = (G_TYPE_CHECK_INSTANCE_CAST (mip->modinst_handle, api_type, GObject));
    *p_iface = (G_TYPE_INSTANCE_GET_INTERFACE (mip->modinst_handle, api_type, GTypeInterface));
    if ((NULL == *p_api) || (NULL == *p_iface)) {
      *p_api = NULL;
      *p_iface = NULL;
      goto done;
    }     
    rw_status = RW_STATUS_SUCCESS;
  }
  
done:

  va_end(var_args);
  
  return rw_status;
}



/*
 * Return the PEAS extension used to provide a module instance
 */
PeasExtension *
rw_vx_modinst_get_peas_extension(rw_vx_modinst_common_t *mip)
{
  RW_ASSERT(RW_VX_MODINST_VALID(mip));
  RW_ASSERT(RW_VX_MODULE_VALID(mip->modp));
  RW_ASSERT(NULL != mip->modp->peas_extension);

  return mip->modp->peas_extension;
}



/*
 * Add a pin object to a port using the default blockid
 */
rw_vx_port_pin_t *
rw_vx_port_add_pin(rw_vx_plug_port_t *port,
                     RwpluginPinDirection pin_dir,
                     GType api_type,
                     void **p_api,
                     void **p_iface)
{
  return rw_vx_port_add_pin_with_blockid(port,pin_dir,api_type,p_api,p_iface,RW_VX_BLOCKID_DEFAULT);
}



/*
 * Add a pin object to a port using the specified blockid
 */
rw_vx_port_pin_t *
rw_vx_port_add_pin_with_blockid(rw_vx_plug_port_t *port,
                                  RwpluginPinDirection pin_dir,
                                  GType api_type,
                                  void **p_api,
                                  void **p_iface,
                                  uint32_t blockid)
{
  rw_vx_port_pin_t *pin = NULL;
  RW_ASSERT(RW_VX_PORT_VALID(port));
  RW_ASSERT(RW_VX_PLUG_VALID(port->plug));
  RW_ASSERT((pin_dir == RWPLUGIN_PIN_DIRECTION_INPUT) || (pin_dir == RWPLUGIN_PIN_DIRECTION_OUTPUT));
  RW_ASSERT(0 != api_type);
  RW_ASSERT((p_api && p_iface) || (!p_api && !p_iface));

  /* SET LINK RESULTS TO NULL */
  if (p_api) {
    *p_api = NULL;
  }
  if (p_iface) {
    *p_iface = NULL;
  }
  
  /*
   * An output pin can only provide a supported API
   */

  pin = RW_MALLOC0(sizeof(*pin));
  pin->magic = RW_VX_PIN_MAGIC;
  pin->pin_dir = pin_dir;
  pin->api_type = api_type;
  pin->api_name = g_type_name(api_type);
  pin->port = port;
  pin->p_api = p_api; // OK to be NULL
  pin->p_iface = p_iface; // OK to be NULL

  /*
   * Insert pin into the appropriate list at the plug level
   * and perform any required checks
   */
  switch(pin_dir) {
    case RWPLUGIN_PIN_DIRECTION_INPUT:
      RW_DL_INSERT_TAIL(&port->plug->input_pin_list,pin,ft_dl_elem);
      break;
      
    case RWPLUGIN_PIN_DIRECTION_OUTPUT:
      {
        if (port->mip->modp->plugin_info) {
          if (!peas_engine_provides_extension(port->mip->rw_vxfp->engine,
                                              port->mip->modp->plugin_info,
                                              api_type)) {
            printf("rw_vx_port_add_pin_with_blockid() ERROR: Attempt to add output pin on port using unsupported API %u (%s)\n",
                   (unsigned int)api_type,g_type_name(api_type));
            pin->magic = RW_VX_BAD_MAGIC;
            RW_FREE(pin);
            pin = NULL;
            goto done;
          }
        }
        else {
          /* Pseudo-module providing interfaces */
          uint32_t i;
          guint n_iftypes;
          GType *iftypes;
          RW_ASSERT(0 != port->mip->pseudo_module_GType);
          // Get the list of interfaces supported by the pseudo-modinst
          iftypes = g_type_interfaces(port->mip->pseudo_module_GType,&n_iftypes);
          RW_ASSERT(iftypes);
          for(i=0;i<n_iftypes;i++) {
            if (iftypes[i] == api_type) {
              break;
            }
          }
          g_free(iftypes);
          if (i == n_iftypes) {
            /* API not found */
            printf("rw_vx_port_add_pin_with_blockid() ERROR: Attempt to add output pin on port using unsupported API %u (%s)\n",
                   (unsigned int)api_type,g_type_name(api_type));
            pin->magic = RW_VX_BAD_MAGIC;
            RW_FREE(pin);
            pin = NULL;
            goto done;
          }
        }
        /* Looks good */
        RW_DL_INSERT_TAIL(&port->plug->output_pin_list,pin,ft_dl_elem);
      }
      break;
      
    default:
      RW_CRASH();
      break;

  } /* switch(pin_dir) { */

done:
  if (pin) {
    // Insert into port's list of all pins
    RW_DL_INSERT_TAIL(&port->pin_list,pin,dl_elem);
  }
  
  return pin;
}



/*
 * This function does 2 things:
 *
 * 1) For each input pin of EACH plug, find a matching output pin in the other plug
 *    and set then set the input pin's peer_pin to that output pin
 * 2) For each peer_pin, set the input pin's api and interface pointers
 */
static void
rw_vx_link_match_helper(char *idstr,
                          rw_vx_plug_t *plug1,
                          rw_vx_plug_t *plug2,
                          rw_vx_link_handle_t *link_handle)
{
  rw_vx_port_pin_t *opin, *ipin;
  rw_vx_plug_port_t *iport;
  bool_t match_found;
  RW_ASSERT(idstr);
  RW_ASSERT(RW_VX_PLUG_VALID(plug1));
  RW_ASSERT(RW_VX_PLUG_VALID(plug2));
  RW_ASSERT(RW_VX_LINK_HANDLE_VALID(link_handle));

  // Find a match for each input pin of PLUG1
  for(ipin = RW_DL_HEAD(&plug1->input_pin_list,rw_vx_port_pin_t,ft_dl_elem);
      (NULL != ipin);
      ipin = RW_DL_NEXT(ipin,rw_vx_port_pin_t,ft_dl_elem)) {
    iport = ipin->port;
    RW_ASSERT(RW_VX_PORT_VALID(iport));
    
    // Search the output pins of the PLUG2
    match_found = FALSE;
    for(opin = RW_DL_HEAD(&plug2->output_pin_list,rw_vx_port_pin_t,ft_dl_elem);
        (NULL != opin);
        opin = RW_DL_NEXT(opin,rw_vx_port_pin_t,ft_dl_elem)) {
      RW_ASSERT(RW_VX_PORT_VALID(opin->port));
      
      /*
       * If a input pin specifies a specific blockid,
       * then the output pin's port must also have that same blockid
       */
      if ((ipin->blockid != RW_VX_BLOCKID_DEFAULT) &&
          (ipin->blockid != opin->port->blockid)) {
        continue; /* Port doesn't match the connector pin */
      }
      if (ipin->api_type == opin->api_type) {
        // printf("rw_vx_link(): %s found matching api %u %s\n",idstr,(unsigned int)ipin->api_type,ipin->api_name);
        match_found = TRUE;
        break;
      }
    } /* each each output pin of the input plug */
    
    if (FALSE == match_found) {
      link_handle->unlinked_input_pins++;
      link_handle->status = RW_STATUS_FAILURE;
      printf("rw_vx_link() ERROR: %s NO matching api %u %s\n",idstr,(unsigned int)ipin->api_type,ipin->api_name);
      continue;
    }

    /* Make sure the output pin has valid info */
    RW_ASSERT(RW_VX_PIN_VALID(opin));
    RW_ASSERT(opin->linkinfo.api);
    RW_ASSERT(opin->linkinfo.iface);

    /*
     * Assign the selected output pin to the input pin
     * and then update the api and iface pointers
     * for the installer of the input pin
     */
    ipin->linkinfo.peer_pin = opin;

    if (ipin->p_api) {
      *ipin->p_api = opin->linkinfo.api;
    }
    if (ipin->p_iface) {
      *ipin->p_iface = opin->linkinfo.iface;
    }
    
    /*
     * Update some debugging info in the output pin
     */
    opin->linkinfo.use_count++;
    opin->linkinfo.peer_pin = ipin;

  } /* for each input pin of PLUG1 */
}


/*
 * Helper type for function rw_vx_link_helper()
 */
typedef enum {
  RW_VX_LINK_PHASE_PRELINK_CHECK,
  RW_VX_LINK_PHASE_PRELINK,
  RW_VX_LINK_PHASE_POSTLINK,
  RW_VX_LINK_PHASE_PREUNLINK,
  RW_VX_LINK_PHASE_POSTUNLINK,
} rw_vx_link_phase_t;



/*
 * This function is used to perform various function calls on each input pin for a plug
 * The function it calls depends on the link-phase "lphase"
 */
static void
rw_vx_link_helper(rw_vx_link_phase_t lphase,
                    char *idstr,
                    uint32_t linkid,
                    rw_vx_plug_t *plug,
                    rw_vx_link_handle_t *link_handle)
{
  rw_vx_port_pin_t *ipin, *opin;
  rw_vx_modinst_common_t *omip, *imip;
  // rw_vx_module_common_t *omodp;
  rw_vx_module_common_t *imodp;
  RW_ASSERT(idstr);
  RW_ASSERT(RW_VX_PLUG_VALID(plug));
  RW_ASSERT(RW_VX_LINK_HANDLE_VALID(link_handle));

  /*
   * For each each input pin, verify the output pin is good to link
   */
  for(ipin = RW_DL_HEAD(&plug->input_pin_list,rw_vx_port_pin_t,ft_dl_elem);
      (NULL != ipin);
      ipin = RW_DL_NEXT(ipin,rw_vx_port_pin_t,ft_dl_elem)) {
    RW_ASSERT(RW_VX_PIN_VALID(ipin));
    opin = ipin->linkinfo.peer_pin;
    RW_ASSERT(RW_VX_PIN_VALID(opin));

    RW_ASSERT(RW_VX_PORT_VALID(ipin->port));
    imip = ipin->port->mip;
    RW_ASSERT(RW_VX_MODINST_VALID(imip));
    imodp = imip->modp;
    RW_ASSERT(RW_VX_MODULE_VALID(imodp));

    RW_ASSERT(RW_VX_PORT_VALID(opin->port));
    omip = opin->port->mip;
    RW_ASSERT(RW_VX_MODINST_VALID(omip));

    /*
     * Call the appropriate link-phase specific function
     */
    switch (lphase) {

      case RW_VX_LINK_PHASE_PRELINK_CHECK:
        {
          gboolean prelink_ok;
          RW_ASSERT(imodp->ci_interface->module_instance_prelink_check);
          RW_ASSERT(imodp->ci_api);
          prelink_ok = imodp->ci_interface->module_instance_prelink_check(imodp->ci_api,
                                                                          imip->modinst_handle,
                                                                          linkid,
                                                                          rw_vx_modinst_iface_provider(omip), // interface provider
                                                                          omip->modinst_handle,
                                                                          opin->api_type,
                                                                          opin->api_name);
          if (!prelink_ok) {
            link_handle->prelink_check_fail_count++;
          }
        }
        break;

      case RW_VX_LINK_PHASE_PRELINK:
        {
          RW_ASSERT(imodp->ci_interface->module_instance_prelink);
          RW_ASSERT(imodp->ci_api);
          imodp->ci_interface->module_instance_prelink(imodp->ci_api,
                                                       imip->modinst_handle,
                                                       linkid,
                                                       rw_vx_modinst_iface_provider(omip), // interface provider
                                                       omip->modinst_handle,
                                                       opin->api_type,
                                                       opin->api_name);
        }
        break;

      case RW_VX_LINK_PHASE_POSTLINK:
        {
          RW_ASSERT(imodp->ci_interface->module_instance_postlink);
          RW_ASSERT(imodp->ci_api);
          imodp->ci_interface->module_instance_postlink(imodp->ci_api,
                                                        imip->modinst_handle,
                                                        linkid,
                                                        rw_vx_modinst_iface_provider(omip), // interface provider
                                                        omip->modinst_handle,
                                                        opin->api_type,
                                                        opin->api_name);          
        }
        break;

      case RW_VX_LINK_PHASE_PREUNLINK:
        {
          RW_ASSERT(imodp->ci_interface->module_instance_preunlink);
          RW_ASSERT(imodp->ci_api);
          imodp->ci_interface->module_instance_preunlink(imodp->ci_api,
                                                         imip->modinst_handle,
                                                         linkid,
                                                         rw_vx_modinst_iface_provider(omip), // interface provider
                                                         omip->modinst_handle,
                                                         opin->api_type,
                                                         opin->api_name);
        }
        break;

      case RW_VX_LINK_PHASE_POSTUNLINK:
        {
          RW_ASSERT(imodp->ci_interface->module_instance_postunlink);
          RW_ASSERT(imodp->ci_api);
          imodp->ci_interface->module_instance_postunlink(imodp->ci_api,
                                                          imip->modinst_handle,
                                                          linkid,
                                                          rw_vx_modinst_iface_provider(omip), // interface provider
                                                          omip->modinst_handle,
                                                          opin->api_type,
                                                          opin->api_name);
        }
        break;

      default:
        RW_CRASH();
        break;
    }
  }
}



/*
 * This function tries to link the module-instances
 * in "plug1" to the module-instances in the "plug2"
 * using the APIs in the ports/pins
 */
static rw_vx_link_handle_t *
rw_vx_plug_linker(uint32_t linkid,
                    rw_vx_plug_t *plug1,
                    rw_vx_plug_t *plug2)
{
  rw_vx_port_pin_t *opin;
  rw_vx_link_handle_t *link_handle;
  /* Sanity-check plug1 and plug2 */
  RW_ASSERT(RW_VX_PLUG_VALID(plug1));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(plug1->rw_vxfp));
  // NOTE: plug2 is optional
  RW_ASSERT(!plug2 || RW_VX_PLUG_VALID(plug2));
  RW_ASSERT(!plug2 || RW_VX_FRAMEWORK_VALID(plug2->rw_vxfp));
  RW_ASSERT(!plug2 || (!plug1 || (plug1->rw_vxfp == plug2->rw_vxfp)));


  link_handle = RW_MALLOC0(sizeof(*link_handle));
  link_handle->magic = RW_VX_LINK_HANDLE_MAGIC;
  link_handle->linkid = linkid;
  link_handle->rw_vxfp = plug1->rw_vxfp;
  link_handle->plug1 = plug1;
  link_handle->plug2 = plug2;
  link_handle->status = RW_STATUS_SUCCESS;
  RW_DL_INSERT_TAIL(&plug1->rw_vxfp->link_handle_list,link_handle,dl_elem);

  if (plug1) {

    if (plug1->linked) {
      printf("rw_vx_link() ERROR: plug1 already linked\n");
      link_handle->status = RW_STATUS_FAILURE;
      goto done;
    }
    
    /*
     * PLUG1 should have no input pins if PLUG2==NULL
     */
    if (!plug2 && (RW_DL_LENGTH(&plug1->input_pin_list) > 0)) {
      link_handle->unlinked_input_pins += RW_DL_LENGTH(&plug1->input_pin_list);
      link_handle->status = RW_STATUS_FAILURE;
      printf("rw_vx_link() ERROR: plug1 has input pins, but plug2 was NULL");
    }

    /*
     * Setup output pins of PLUG1
     */
    for(opin = RW_DL_HEAD(&plug1->output_pin_list,rw_vx_port_pin_t,ft_dl_elem);
        (NULL != opin);
        opin = RW_DL_NEXT(opin,rw_vx_port_pin_t,ft_dl_elem)) {
      RW_ASSERT(RW_VX_PORT_VALID(opin->port));
      opin->linkinfo.api = (G_TYPE_CHECK_INSTANCE_CAST (rw_vx_modinst_iface_provider(opin->port->mip), opin->api_type, GObject));
      RW_ASSERT(opin->linkinfo.api);
      if (opin->p_api) {
        *opin->p_api = opin->linkinfo.api;
      }
      opin->linkinfo.iface = (G_TYPE_INSTANCE_GET_INTERFACE (rw_vx_modinst_iface_provider(opin->port->mip), opin->api_type, GTypeInterface));
      RW_ASSERT(opin->linkinfo.iface);
      if (opin->p_iface) {
        *opin->p_iface = opin->linkinfo.iface;
      }
    }
  } /*   if (plug1) { */
  
  if (plug2) {

    if (plug2->linked) {
      printf("rw_vx_link() ERROR: plug2 already linked\n");
      link_handle->status = RW_STATUS_FAILURE;
      goto done;
    }

    /*
     * PLUG2 should have no input pins if PLUG1==NULL
     */
    if (!plug1 && (RW_DL_LENGTH(&plug2->input_pin_list) > 0)) {
      link_handle->unlinked_input_pins += RW_DL_LENGTH(&plug2->input_pin_list);
      link_handle->status = RW_STATUS_FAILURE;
      printf("rw_vx_link() ERROR: plug2 has as input pins, but plug1 was NULL");
    }

    /*
     * Setup output pins of PLUG2
     */
    for(opin = RW_DL_HEAD(&plug2->output_pin_list,rw_vx_port_pin_t,ft_dl_elem);
        (NULL != opin);
        opin = RW_DL_NEXT(opin,rw_vx_port_pin_t,ft_dl_elem)) {
      RW_ASSERT(RW_VX_PORT_VALID(opin->port));
      opin->linkinfo.api = (G_TYPE_CHECK_INSTANCE_CAST (rw_vx_modinst_iface_provider(opin->port->mip), opin->api_type, GObject));
      RW_ASSERT(opin->linkinfo.api);
      if (opin->p_api) {
        *opin->p_api = opin->linkinfo.api;
      }
      opin->linkinfo.iface = (G_TYPE_INSTANCE_GET_INTERFACE (rw_vx_modinst_iface_provider(opin->port->mip), opin->api_type, GTypeInterface));
      RW_ASSERT(opin->linkinfo.iface);
      if (opin->p_iface) {
        *opin->p_iface = opin->linkinfo.iface;
      }
    }
  }  /*   if (plug2) { */
  
  /*
   * If linking 2 plugs, then VERIFY each INPUT pin has a corresponding OUTPUT pin in the OTHER PLUG
   */
  if (plug1 && plug2) {
    
    rw_vx_link_match_helper("plug1",plug1,plug2,link_handle);
    rw_vx_link_match_helper("plug2",plug2,plug1,link_handle);

    /*
     * If an error has been detected, finish up
     */
    if (RW_STATUS_SUCCESS != link_handle->status) {
      goto done;
    }

    /*
     * Prelink-check each plug's pins
     */
    rw_vx_link_helper(RW_VX_LINK_PHASE_PRELINK_CHECK,"plug1",linkid,plug1,link_handle);
    rw_vx_link_helper(RW_VX_LINK_PHASE_PRELINK_CHECK,"plug2",linkid,plug2,link_handle);

    /*
     * If an error has been detected, finish up
     */
    if (RW_STATUS_SUCCESS != link_handle->status) {
      goto done;
    }

    /*
     * Prelink each plug's pins
     */      
    rw_vx_link_helper(RW_VX_LINK_PHASE_PRELINK,"plug1",linkid,plug1,link_handle);
    rw_vx_link_helper(RW_VX_LINK_PHASE_PRELINK,"plug2",linkid,plug2,link_handle);
    
    /*
     * Postlink each plug's pins
     */
    rw_vx_link_helper(RW_VX_LINK_PHASE_POSTLINK,"plug1",linkid,plug1,link_handle);
    rw_vx_link_helper(RW_VX_LINK_PHASE_POSTLINK,"plug2",linkid,plug2,link_handle);

    if (RW_STATUS_SUCCESS == link_handle->status) {
      plug1->linked = plug2->linked = TRUE;
      link_handle->successfully_linked = TRUE;
    }
    
  }

done:

  if (link_handle->unlinked_output_pins || link_handle->unlinked_input_pins) {
    printf("rw_vx_link() WARNING: unlinked_output_pins=%u  unlinked_input_pins=%u\n",
           link_handle->unlinked_output_pins,
           link_handle->unlinked_input_pins);
  }

  if (link_handle->prelink_check_fail_count) {
    printf("rw_vx_link() ERROR: prelink_check_fail_count == %u\n",
           link_handle->prelink_check_fail_count);
    RW_ASSERT(RW_STATUS_SUCCESS != link_handle->status);
  }

  if (RW_STATUS_SUCCESS != link_handle->status) {
    printf("rw_vx_link() ERROR: fail status=%u\n",link_handle->status);
  }

  RW_ASSERT(RW_VX_LINK_HANDLE_VALID(link_handle));
  return link_handle;
}



/*
 * Link 2 plugs
 */
rw_vx_link_handle_t *
rw_vx_link2(uint32_t linkid,
              rw_vx_plug_t *plug1,
              rw_vx_plug_t *plug2)
{
  RW_ASSERT(RW_VX_PLUG_VALID(plug1));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(plug1->rw_vxfp));
  RW_ASSERT(RW_VX_PLUG_VALID(plug2));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(plug2->rw_vxfp));

  return rw_vx_plug_linker(linkid,plug1,plug2);
}



/*
 * Link 1 plug
 */
rw_vx_link_handle_t *
rw_vx_link1(uint32_t linkid,
              rw_vx_plug_t *plug1)
{
  RW_ASSERT(RW_VX_PLUG_VALID(plug1));
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(plug1->rw_vxfp));

  return rw_vx_plug_linker(linkid,plug1,NULL);
}



/*
 * Release a link_handle and the associated plugs/pins
 */
rw_status_t
rw_vx_unlink(rw_vx_link_handle_t *link_handle)
{
  RW_ASSERT(RW_VX_LINK_HANDLE_VALID(link_handle));

  if (link_handle->successfully_linked) {
    RW_ASSERT(RW_VX_PLUG_VALID(link_handle->plug1));
    RW_ASSERT(RW_VX_PLUG_VALID(link_handle->plug2));
    RW_ASSERT(link_handle->plug1->linked);
    RW_ASSERT(link_handle->plug2->linked);

    /*
     * Pre-unlink each plug's INPUT pins
     */      
    rw_vx_link_helper(RW_VX_LINK_PHASE_PREUNLINK,"plug1",link_handle->linkid,link_handle->plug1,link_handle);
    rw_vx_link_helper(RW_VX_LINK_PHASE_PREUNLINK,"plug2",link_handle->linkid,link_handle->plug2,link_handle);
    
    /*
     * Post-unlink each plug's INPUT pins
     */
    rw_vx_link_helper(RW_VX_LINK_PHASE_POSTUNLINK,"plug1",link_handle->linkid,link_handle->plug1,link_handle);
    rw_vx_link_helper(RW_VX_LINK_PHASE_POSTUNLINK,"plug2",link_handle->linkid,link_handle->plug2,link_handle);

    /*
     * Cleanup
     */
    link_handle->plug1->linked = FALSE;
    link_handle->plug2->linked = FALSE;
  }

  if (link_handle->plug1) {
    rw_vx_plug_free(link_handle->plug1);
    link_handle->plug1 = NULL;
  }

  if (link_handle->plug2) {
    rw_vx_plug_free(link_handle->plug2);
    link_handle->plug2 = NULL;
  }

  RW_DL_REMOVE(&link_handle->rw_vxfp->link_handle_list,link_handle,dl_elem);
  link_handle->magic = RW_VX_BAD_MAGIC;
  RW_FREE(link_handle);
  
  return RW_STATUS_SUCCESS;
}

void rw_vx_prepend_gi_search_path(const char * path)
{
  g_irepository_prepend_search_path(path);
}

void rw_vx_add_peas_search_path(
    rw_vx_framework_t * vxfp,
    const char * path)
{
  peas_engine_add_search_path(vxfp->engine, path, path);
}

void rw_vx_require_repository(
    const char * ns,
    const char * version)
{
  GError * error = NULL;

  g_irepository_require(
      g_irepository_get_default(),
      ns,
      version,
      (GIRepositoryLoadFlags)0,
      &error);
  g_assert_no_error(error);

  return;
}

static void
rw_vx_pseudo_modinst_free(RwpluginApi *self,
                            GObject* module_instance_handle)
{
  rw_vx_pseudo_module_instance_t *pmp = (rw_vx_pseudo_module_instance_t *)module_instance_handle;
  RW_ASSERT(self);
  RW_ASSERT(module_instance_handle);

  g_object_unref(&pmp->gobject); 
}

static gboolean
rw_vx_pseudo_module_instance_prelink_check(RwpluginApi *api,
                                             GObject *module_instance_handle,
                                             guint32 linkid,
                                             GObject *iface_provider,
                                             GObject *iface_mih,
                                             guint32 apitype,
                                             const gchar *apistr)
{
  //ZZZ
  //RW_CRASH();
  return TRUE;
}
static void
rw_vx_pseudo_module_instance_prelink(RwpluginApi *api,
                                       GObject *module_instance_handle,
                                       guint32 linkid,
                                       GObject *iface_provider,
                                       GObject *iface_mih,
                                       guint32 apitype,
                                       const gchar *apistr)
{
  //ZZZ
  //RW_CRASH();
}
static void
rw_vx_pseudo_module_instance_postlink(RwpluginApi *api,
                                        GObject *module_instance_handle,
                                        guint32 linkid,
                                        GObject *iface_provider,
                                        GObject *iface_mih,
                                        guint32 apitype,
                                        const gchar *apistr)
{
  //ZZZ
  //RW_CRASH();
}
static void
rw_vx_pseudo_module_instance_preunlink(RwpluginApi *api,
                                         GObject *module_instance_handle,
                                         guint32 linkid,
                                         GObject *iface_provider,
                                         GObject *iface_mih,
                                         guint32 apitype,
                                         const gchar *apistr)
{
  //ZZZ
  //RW_CRASH();
}
static void
rw_vx_pseudo_module_instance_postunlink(RwpluginApi *api,
                                          GObject *module_instance_handle,
                                          guint32 linkid,
                                          GObject *iface_provider,
                                          GObject *iface_mih,
                                          guint32 apitype,
                                          const gchar *apistr)
{
  //ZZZ
  //RW_CRASH();
}

                               
rw_vx_modinst_common_t *
rw_vx_pseudo_module_instance(rw_vx_framework_t *rw_vxfp,
                               void *priv)
{
  rw_vx_modinst_common_t *mip;
  rw_status_t rw_status;
  char rw_vx_pseudo_module_name[] = RW_VX_PSEUDO_MODULE_NAME;
  gchar *gobj_type_name;
  GTypeInfo tm_obj_info;
  GInterfaceInfo info;
  rw_vx_pseudo_module_instance_t *pmodi;
  static uint32_t rw_vx_pseudo_module_last_minor;
  RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  RW_ASSERT(NULL != priv);

  gobj_type_name = RW_STRDUP_PRINTF("%s-type-%u",
                                      rw_vx_pseudo_module_name,
                                      rw_vx_pseudo_module_last_minor);
  RW_ASSERT(gobj_type_name);
  
  rw_status = rw_vx_modinst_open(rw_vxfp,
                                     rw_vx_pseudo_module_name,
                                     rw_vx_pseudo_module_last_minor,
                                     "",
                                     &mip);
  RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
  rw_vx_pseudo_module_last_minor++;


  RW_ZERO_VARIABLE(&tm_obj_info);
  tm_obj_info.class_size = sizeof(GObjectClass);
  tm_obj_info.instance_size = sizeof(rw_vx_pseudo_module_instance_t);
  mip->pseudo_module_GType = g_type_register_static(G_TYPE_OBJECT,gobj_type_name,&tm_obj_info,0);
  RW_ASSERT(0 != mip->pseudo_module_GType);
  g_free(gobj_type_name);
  gobj_type_name = NULL;

  pmodi = g_object_new(mip->pseudo_module_GType,NULL);
  RW_ASSERT(pmodi);
  pmodi->priv = priv;
  mip->modinst_handle = (GObject *)pmodi;

  RW_ZERO_VARIABLE(&info); // This is the init/finalize/data for the interface
  g_type_add_interface_static(mip->pseudo_module_GType,RWPLUGIN_TYPE_API,&info);

  mip->modp->ci_api = (G_TYPE_CHECK_INSTANCE_CAST (mip->modinst_handle, RWPLUGIN_TYPE_API, RwpluginApi));
  RW_ASSERT(mip->modp->ci_api);
  mip->modp->ci_interface = (G_TYPE_INSTANCE_GET_INTERFACE (mip->modinst_handle, RWPLUGIN_TYPE_API, RwpluginApiIface));
  RW_ASSERT(mip->modp->ci_interface);
  mip->modp->ci_interface->module_init = NULL;
  mip->modp->ci_interface->module_deinit = NULL;
  mip->modp->ci_interface->module_instance_alloc = NULL;
  mip->modp->ci_interface->module_instance_free = rw_vx_pseudo_modinst_free;
  mip->modp->ci_interface->module_instance_prelink_check = rw_vx_pseudo_module_instance_prelink_check;
  mip->modp->ci_interface->module_instance_prelink = rw_vx_pseudo_module_instance_prelink;
  mip->modp->ci_interface->module_instance_postlink = rw_vx_pseudo_module_instance_postlink;
  mip->modp->ci_interface->module_instance_preunlink = rw_vx_pseudo_module_instance_preunlink;
  mip->modp->ci_interface->module_instance_postunlink = rw_vx_pseudo_module_instance_postunlink;

  return mip;
}

void
rw_vx_pseudo_modinst_add_interface(rw_vx_modinst_common_t *mip,
                                     GType api_type,
                                     void **p_iface)
{
  GInterfaceInfo info;
  RW_ASSERT(RW_VX_MODINST_VALID(mip));
  RW_ASSERT(api_type); // Adding a valid API type
  RW_ASSERT(p_iface);
  RW_ASSERT(0 != mip->pseudo_module_GType); // MUST be !=0 if a pseudo-module

  RW_ZERO_VARIABLE(&info); // This is the init/finalize/data for the interface
  g_type_add_interface_static(mip->pseudo_module_GType,api_type,&info);

  *p_iface = (G_TYPE_INSTANCE_GET_INTERFACE (mip->modinst_handle, api_type, void *));
  RW_ASSERT(*p_iface);
}


/*
 * TODO PLAN:
 *  - cleanup/fix the linkid stuff
 */

#if 0
{
  GType ext_gtype;
  uint32_t i;
  guint                n_iftypes;
  GType               *iftypes;   /**< array of interface types supported by the the module instance */

  RW_ASSERT(rw_vx_modinst_iface_provider(mip));
  // Obtain GType of the PEAS extension for the module from which the module instance (mip) allocated
  ext_gtype = G_TYPE_FROM_INSTANCE(rw_vx_modinst_iface_provider(mip));
  // Get the list of interfaces supported by the module
  iftypes = g_type_interfaces(ext_gtype,&n_iftypes);
  RW_ASSERT(iftypes);

  printf("GType=%s nifs=%u:",g_type_name(ext_gtype),n_iftypes);
  for(i=0;i<n_iftypes;i++) {
    printf(" %s",g_type_name(iftypes[i]));
  }
  printf("\n");

  g_free(iftypes);
}
#endif /* 0 */
