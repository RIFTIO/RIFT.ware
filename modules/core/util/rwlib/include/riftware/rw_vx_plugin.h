
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
 * @file rw_vx_plugin.h
 * @author Matt Harper (matt.harper@riftio.com)
 * @date 07/24/2013
 * @brief RiftWare Virtual Executive plug/port/pin framework header file
 * 
 * @details Include this header file to manipulate RIFT plugins
 */
#if !defined(__RW_VX_PLUGIN_H__)
#define __RW_VX_PLUGIN_H__

#include "rwlib.h"
#include <rw_sklist.h>
#include <rw_dl.h>

#include <libpeas/peas.h>
#include "rwplugin.h"

__BEGIN_DECLS

#define RW_VX_BAD_MAGIC                   0xdeadbeef /* A common invalid magic # */
#define RW_VX_FRAMEWORK_MAGIC             0xdade0120
#define RW_VX_MODULE_MAGIC                0xdad00121
#define RW_VX_MODINST_MAGIC               0xdad00122
#define RW_VX_PLUG_MAGIC                  0xdade0123
#define RW_VX_PORT_MAGIC                  0xdade0124
#define RW_VX_PIN_MAGIC                   0xdade0125
#define RW_VX_LINK_HANDLE_MAGIC           0xdade0126

#define RW_VX_PSEUDO_MODULE_NAME "RW_VX_INTERNAL_PSEUDO_MODULE_NAME"

/* forward decls */
struct _rw_vx_port_pin;

typedef enum {
  RW_VX_GLOBAL_LOADERS,
  RW_VX_NONGLOBAL_LOADERS
} rw_vx_loader_type_t;

/**
 * This type is the handle to a RW_VX framework
 */
typedef struct {
  uint32_t      magic;            /**< RW_VX_FRAMEWORK_MAGIC */
  rw_vx_loader_type_t loader_type;
  PeasEngine    *engine;          /**< Loadable Modules engine */
  rw_sklist_t module_sklist;    /**< skiplist of ALL registered modules */
  rw_dl_t     plug_list;        /**< DLL of all plugs */
  rw_dl_t     link_handle_list; /**< DLL of all link handles */
} rw_vx_framework_t;
#define RW_VX_FRAMEWORK_VALID(_p)  ((NULL != (_p)) && (RW_VX_FRAMEWORK_MAGIC == (_p)->magic))



/**
 * Definition for the common part of every module-specific stucture
 * that comes before anything else in the type's struct defintion
 */
typedef struct {
  uint32_t            magic;                /**< RW_VX_MODULE_MAGIC */
  rw_vx_framework_t *rw_vxfp;           /**< RW_VX framework */
  char                *module_name;         /**< Name of this module */
  char                *module_alloc_cfg;    /**< Config required to allocate the module */

  PeasExtension               *peas_extension;       /**< PEAS extension handle (aka plugin) for the module */
  PeasPluginInfo              *plugin_info;          /**< PEAS pluigin info for the module */
  RwpluginApi      *ci_api;               /**< Common interface API handle */
  RwpluginApiIface *ci_interface;         /**< Common interface function pointers */
  GObject                     *module_handle;        /**< handle returned by cf_interface->module_init() */

  rw_sklist_t               all_modinsts_sklist;   /**< List of all modinsts by minor for this module */
  rw_sklist_element_t       module_sklist_element; /**< for instance->modules_sklist */
} rw_vx_module_common_t;
#define RW_VX_MODULE_VALID(_p)  ((NULL != (_p)) && (RW_VX_MODULE_MAGIC == (_p)->magic))



/**
 * Definition for the common part of every modinst-specific stucture
 * that comes before anything else in the type's struct defintion
 */
typedef struct {
  GObject                 *modinst_handle;        /**< module instance handle provided by plugin */
  uint32_t                magic;                  /**< RW_VX_MODINST_MAGIC */
  uint32_t                minor;                  /**< Minor modinst instance # */
  uint32_t                port_refs;              /**< # of rw_vx_plug_port_t refs to this */
  char                    *alloc_cfg;             /**< Config passed during open() */
  rw_vx_framework_t     *rw_vxfp;             /**< RW_VX framework used to create this modinst */
  rw_vx_module_common_t *modp;                  /**< Ptr back to the module for this modinst */
  GType                   pseudo_module_GType;    /**< if this is a pseudo-module, the corresponding GType (!= 0) */
  uint32_t                used_as_library_count;  /**< >0 if this module instance is being treated as a library */

  rw_sklist_element_t all_modinsts_sklist_elem; /**< for module->all_modinsts_sklist */
} rw_vx_modinst_common_t;
#define RW_VX_MODINST_VALID(_p)  ((NULL != (_p)) && (RW_VX_MODINST_MAGIC == (_p)->magic))


/**
 * The plug object represents interfaces imported/exported between GObjects
 * They are linked/unlinked cia the rw_vx_link2(), rw_vx_link1(), and rw_vx_unlink() calls
 *
 * Plugs contain a list of ports that correspond to PEAS loadable modules
 */
typedef struct {
  uint32_t            magic;           /**< RW_VX_PLUG_MAGIC */
  gboolean            linked;          /**< Is this plug already linked ? */
  rw_vx_framework_t *rw_vxfp;      /**< framework that owns this plug */
  rw_dl_t           port_list;       /**< list of all ports associataed with this plug */
  rw_dl_t           input_pin_list;  /**< list of ALL to pinds in the plug */
  rw_dl_t           output_pin_list; /**< list of ALL output pins in the plug */

  rw_dl_element_t   dl_elem;         /**< threading element for each plug in the rw_vxfp */
} rw_vx_plug_t;
#define RW_VX_PLUG_VALID(_plug)  ((NULL != (_plug)) && (RW_VX_PLUG_MAGIC == (_plug)->magic))

/**
 *
 */
typedef struct {
  GObject gobject; /**< Gobject that providing the interfaces of the pseudo-module instance */
  void    *priv;   /**< private handle for use by the application */
} rw_vx_pseudo_module_instance_t;



#define RW_VX_BLOCKID_DEFAULT 0


/**
 * The port object correspond to a PEAS loadable module
 *
 * A port contains a list of pins which correspond to inported/exported Interfaces (APIs)
 */
typedef struct {
  uint32_t             magic;     /**< RW_VX_PORT_MAGIC */
  rw_vx_plug_t           *plug;      /**< The plug that owns this port */
  rw_vx_modinst_common_t *mip;       /**< module instance providing interfaces for this port */
  rw_dl_t            pin_list;  /**< List of ALL pins associated with this port */
  uint32_t             blockid;   /**< used to control matching */
  

  rw_dl_element_t    dl_elem;   /**< threading element for each port in the plug */
} rw_vx_plug_port_t;
#define RW_VX_PORT_VALID(_port) ((NULL != (_port)) && (RW_VX_PORT_MAGIC == (_port)->magic))


/**
 * The pin object represents a PEAS module instance's
 * GObject interface (API) exported or required
 */
typedef struct rw_vx_port_pin_t {
  uint32_t                         magic;                /**< RW_VX_PIN_MAGIC */
  RwpluginPinDirection  pin_dir;              /**< direction of this pin */
  uint32_t                         blockid;              /**< used to control matching */
  GType                            api_type;             /**< interface API type */
  const char                       *api_name;            /**< g_type_name(api_type) */
  rw_vx_plug_port_t                   *port;                /**< port this pin is associated with */
  void                             **p_api;              /**< Optional ptr to caller's API handle */
  void                             **p_iface;            /**< Optional ptr to caller's IFACE handle */


  /**<
   * The following info is computed during rw_vx_link()
 */
  struct {
    uint32_t                     use_count;
    struct rw_vx_port_pin_t         *peer_pin;
    GObject                      *api;
    GTypeInterface               *iface;
  } linkinfo;

  /**< Threading elements for various lists */
  rw_dl_element_t              dl_elem;              /**< for rw_vx_plug_port_t pin_list */
  rw_dl_element_t              ft_dl_elem;           /**< for rw_vx_plug_t input_pin_list & output_pin_list */
} rw_vx_port_pin_t;
#define RW_VX_PIN_VALID(_pin) ((NULL != (_pin)) && (RW_VX_PIN_MAGIC == (_pin)->magic))

/**
 * This type represents linked plug(s)
 * The handle is returned by the link() routines and is used to unlink() plugs
 */
typedef struct {
  uint32_t         magic;          /**< RW_VX_LINK_HANDLE_MAGIC */
  
  uint32_t         linkid;
  rw_vx_framework_t   *rw_vxfp;          /**< framework that owns this plug */
  rw_vx_plug_t        *plug1;
  rw_vx_plug_t        *plug2;

  rw_status_t    status;
  gboolean         successfully_linked;
  uint32_t         unlinked_input_pins;
  uint32_t         unlinked_output_pins;
  uint32_t         prelink_check_fail_count;
  
  /**< Threading elements for various lists */
  rw_dl_element_t              dl_elem;              /**< for rw_vx_framework_t link_handle_list */
} rw_vx_link_handle_t;
#define RW_VX_LINK_HANDLE_VALID(_lh) ((NULL != (_lh)) && (RW_VX_LINK_HANDLE_MAGIC == (_lh)->magic))


/********************** END DECLS ***********************************************/


/********************** PRIVATE APIS ********************************************/
/********************** END PRIVATE APIS ****************************************/



/******************************* EXPORTED APIs **********************************/


rw_vx_framework_t *
rw_vx_framework_alloc();

rw_vx_framework_t *
rw_vx_framework_alloc_nonglobal();

rw_status_t
rw_vx_framework_free(rw_vx_framework_t *rw_vxfp);

rw_status_t
rw_vx_module_register(rw_vx_framework_t *rw_vxfp,
                        char *module_name,
                        char *module_alloc_cfg,
                        char *extension_name,
                        rw_vx_module_common_t **modpp);

rw_status_t
rw_vx_module_unregister(rw_vx_framework_t *rw_vxfp,
                          char *module_name);

rw_vx_module_common_t *
rw_vx_module_lookup(rw_vx_framework_t *rw_vxfp,
                      char *module_name);

#define RW_VX_CLONE_MINOR_MODINST (-1)
rw_status_t
rw_vx_modinst_open(rw_vx_framework_t *rw_vxfp,
                     char *module_name,
                     int32_t minor,
                     char *device_alloc_cfg,
                     rw_vx_modinst_common_t **mipp);

rw_status_t
rw_vx_modinst_close(rw_vx_modinst_common_t *mip);

rw_vx_modinst_common_t *
rw_vx_modinst_lookup(rw_vx_framework_t *rw_vxfp,
                       char *module_name,
                       uint32_t minor);

rw_status_t
rw_vx_modinst_get_api_and_iface(rw_vx_modinst_common_t *mip,
                                  GType api_type,
                                  void **p_api,
                                  void **p_iface,
                                  const gchar *first_property,
                                  ...);

PeasExtension *
rw_vx_modinst_get_peas_extension(rw_vx_modinst_common_t *mip);

#define rw_vx_library_get_api_and_iface rw_vx_modinst_get_api_and_iface
#define rw_vx_library_get_peas_extension rw_vx_modinst_get_peas_extension

rw_status_t
rw_vx_library_open(rw_vx_framework_t *rw_vxfp,
                     char *extension_name,
                     char *device_alloc_cfg,
                     rw_vx_modinst_common_t **mipp);

rw_status_t
rw_vx_library_close(rw_vx_modinst_common_t *mip,
                      gboolean force_unload);


rw_vx_modinst_common_t *
rw_vx_library_lookup(rw_vx_framework_t *rw_vxfp,
                       char *extension_name);



/*
 * Pseudo-module instanced related APIs
 */
rw_vx_modinst_common_t *
rw_vx_pseudo_module_instance(rw_vx_framework_t *rw_vxfp,
                               void *priv);
void
rw_vx_pseudo_modinst_add_interface(rw_vx_modinst_common_t *mip,
                                     GType api_type,
                                     void **p_iface);


/*
 * Plugin Framework Linking APIs
 */

rw_vx_plug_t *rw_vx_plug_alloc(rw_vx_framework_t *rw_vxfp);

void rw_vx_plug_free(rw_vx_plug_t *plug);

rw_vx_plug_port_t *
rw_vx_plug_add_port(rw_vx_plug_t *plug,
                      rw_vx_modinst_common_t *mip);

rw_vx_plug_port_t *
rw_vx_plug_add_port_with_blockid(rw_vx_plug_t *plug,
                                   rw_vx_modinst_common_t *mip,
                                   uint32_t blockid);

rw_vx_port_pin_t *
rw_vx_port_add_pin(rw_vx_plug_port_t *port,
                   RwpluginPinDirection pin_dir,
                   GType api_type,
                   void **p_api,
                   void **p_iface);

rw_vx_port_pin_t *
rw_vx_port_add_pin_with_blockid(rw_vx_plug_port_t *port,
                                RwpluginPinDirection pin_dir,
                                GType api_type,
                                void **p_api,
                                void **p_iface,
                                uint32_t blockid);




rw_vx_link_handle_t *
rw_vx_link2(uint32_t linkid,
              rw_vx_plug_t *plug1,
              rw_vx_plug_t *plug2);

rw_vx_link_handle_t *
rw_vx_link1(uint32_t linkid,
              rw_vx_plug_t *plug1);

rw_status_t
rw_vx_unlink(rw_vx_link_handle_t *link_handle);

/**
 * Prepends directory to the GI typelib search path.
 *
 * @param[in] path  path to prepend
 */
void rw_vx_prepend_gi_search_path(const char * path);

/**
 * Add a path to the libpeas plugin search path
 *
 * @param[in] vxfp  vx instance
 * @param[in] path  path to add
 */
void rw_vx_add_peas_search_path(
    rw_vx_framework_t * vxfp,
    const char * path);

/**
 * Force the namespace repo_dir to be loaded if it isn't already.  If
 * namespace_ is not loaded, this function will search for a ".typelib" file
 * using the repository search path. In addition, a version version of
 * namespace may be specified. If version is not specified, the latest will be
 * used.
 *
 * @param[in] ns        GI namespace to load
 * @param[in] version   Namespace version, may be NULL for latest
 */
void rw_vx_require_repository(
    const char * ns,
    const char * version);
 

/*************************** END EXPORTED APIs **********************************/


__END_DECLS

#endif /* !defined(__RW_VX_PLUGIN_H__) */
