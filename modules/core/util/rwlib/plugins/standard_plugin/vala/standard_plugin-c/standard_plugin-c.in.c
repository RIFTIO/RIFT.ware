
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libpeas/peas.h>

#include "rwplugin.h"

#include "standard_plugin.h"
#include "standard_plugin-c.h"

/*
 * Type to hold a module is:            StandardPluginmodule
 * Type to hold a module instance is:   StandardPluginmoduleInstance
 */

/*
 * Called to allocate/init a module
 */
static GObject *
rwplugin_c_extension_module_init(RwpluginApi *api,
                                 const gchar *module_name,
                                 const gchar *module_alloc_cfg)
{
  StandardPluginmodule *mp;
  g_assert(api);
  g_assert(module_name && *module_name);
  g_assert(module_alloc_cfg);

  // printf("rwplugin_c_extension_module_init(%s) cfg='%s'\n",module_name,module_alloc_cfg);
  mp = standard_plugin_module_new();
  mp->name = g_strdup(module_name);
  mp->alloc_cfg = g_strdup(module_alloc_cfg);
  mp->priv = NULL; /* No extra private data members needed (for now) */

  return (GObject *)mp;
}

/*
 * Called to deinit/free a module
 */
static void
rwplugin_c_extension_module_deinit(RwpluginApi *api,
                                   GObject *module_handle)
{
  StandardPluginmodule *mp = (StandardPluginmodule *)module_handle;
  g_assert(api);
  g_assert(mp);

  // printf("rwplugin_c_extension_module_deinit(%s)\n",mp->name);

  g_object_unref(module_handle);
}

static GObject *
rwplugin_c_extension_module_instance_alloc(RwpluginApi *api,
                                           GObject *module_handle,
                                           const gchar *module_instance_str,
                                           const gchar *module_instance_cfg_str)
{
  StandardPluginmodule *mp = (StandardPluginmodule *)module_handle;
  StandardPluginmoduleInstance *mip;
  g_assert(api);
  g_assert(module_handle);
  g_assert(module_instance_str);
  g_assert(module_instance_cfg_str);

  mip = standard_plugin_module_instance_new();
  mip->name = g_strdup(module_instance_str);
  mip->alloc_cfg = g_strdup(module_instance_cfg_str);
  mip->priv = (void *)mp;

  // printf("rwplugin_c_extension_module_instance_alloc(%s/%s) cfg='%s'\n",
  //       mp->name,mip->name,mip->alloc_cfg);
  
  return (GObject *)mip;
}


static void
rwplugin_c_extension_module_instance_free(RwpluginApi *api,
                                          GObject *module_instance_handle)
{
  StandardPluginmoduleInstance *mip = (StandardPluginmoduleInstance *)module_instance_handle;
  StandardPluginmodule *mp = (StandardPluginmodule *)mip->priv;
  g_assert(api);
  g_assert(mip);

  // printf("rwplugin_c_extension_module_instance_free(%s/%s)\n",
  //        mp->name,mip->name);

  mp->priv = NULL;
  g_object_unref(module_instance_handle);
}

static gboolean
rwplugin_c_extension_module_instance_prelink_check(RwpluginApi *api,
                                                   GObject *module_instance_handle,
                                                   guint32 linkid,
                                                   GObject *iface_provider,
                                                   GObject *iface_mih,
                                                   guint32 apitype,
                                                   const gchar *apistr)
{
  gboolean result = FALSE;
  StandardPluginmoduleInstance *mip = (StandardPluginmoduleInstance *)module_instance_handle;
  StandardPluginmodule *mp = (StandardPluginmodule *)mip->priv;
  g_assert(mp);
  g_assert(api);
  g_assert(mip);
  ((void)linkid);
  ((void)apistr);

  // printf("rwplugin_c_extension_module_instance_prelink_check(%s/%s) %u %s\n",
  //        mp->name,mip->name,linkid,apistr);

  if (apitype == STANDARD_PLUGIN_TYPE_API) {
    StandardPluginApi      *api = STANDARD_PLUGIN_API(iface_provider);
    g_assert(api);
    StandardPluginApiIface *iface = STANDARD_PLUGIN_API_GET_INTERFACE(iface_provider);
    g_assert(iface);
    StandardPluginmoduleInstance *pi_mip = (StandardPluginmoduleInstance *)iface_mih;
    g_assert(pi_mip);
    StandardPluginmodule *pi_mp = (StandardPluginmodule *)pi_mip->priv;
    g_assert(pi_mp);
    // printf("  Prelink-Check: Api=%p iface = %p peer_mip_name=(%s/%s)\n",api,iface,pi_mp->name,pi_mip->name);
    result = TRUE; // API known
  }

  return result;
}

static void
rwplugin_c_extension_module_instance_prelink(RwpluginApi *api,
                                             GObject *module_instance_handle,
                                             guint32 linkid,
                                             GObject *iface_provider,
                                             GObject *iface_mih,
                                             guint32 apitype,
                                             const gchar *apistr)
{
  StandardPluginmoduleInstance *mip = (StandardPluginmoduleInstance *)module_instance_handle;
  StandardPluginmodule *mp = (StandardPluginmodule *)mip->priv;
  g_assert(api);
  g_assert(mip);
  g_assert(mp);

  // printf("rwplugin_c_extension_module_instance_prelink(%s/%s) %u %s\n",
  //        mp->name,mip->name,linkid,apistr);

  if (apitype == STANDARD_PLUGIN_TYPE_API) {
    StandardPluginApi      *api = STANDARD_PLUGIN_API(iface_provider);
    StandardPluginApiIface *iface = STANDARD_PLUGIN_API_GET_INTERFACE(iface_provider);
    g_assert(api);
    g_assert(iface);
    StandardPluginmoduleInstance *pi_mip = (StandardPluginmoduleInstance *)iface_mih;
    g_assert(pi_mip);
    StandardPluginmodule *pi_mp = (StandardPluginmodule *)pi_mip->priv;
    g_assert(pi_mp);
    // printf("  Prelink: Api=%p iface = %p peer_mip_name=(%s/%s)\n",api,iface,pi_mp->name,pi_mip->name);
  }

  return;
}

static void
rwplugin_c_extension_module_instance_postlink(RwpluginApi *api,
                                              GObject *module_instance_handle,
                                              guint32 linkid,
                                              GObject *iface_provider,
                                              GObject *iface_mih,
                                              guint32 apitype,
                                              const gchar *apistr)
{
  StandardPluginmoduleInstance *mip = (StandardPluginmoduleInstance *)module_instance_handle;
  StandardPluginmodule *mp = (StandardPluginmodule *)mip->priv;
  g_assert(api);
  g_assert(mip);
  g_assert(mp);

  // printf("rwplugin_c_extension_module_instance_postlink(%s/%s) %u %s\n",
  //        mp->name,mip->name,linkid,apistr);

  if (apitype == STANDARD_PLUGIN_TYPE_API) {
    StandardPluginApi      *api = STANDARD_PLUGIN_API(iface_provider);
    StandardPluginApiIface *iface = STANDARD_PLUGIN_API_GET_INTERFACE(iface_provider);
    g_assert(api);
    g_assert(iface);
    StandardPluginmoduleInstance *pi_mip = (StandardPluginmoduleInstance *)iface_mih;
    g_assert(pi_mip);
    StandardPluginmodule *pi_mp = (StandardPluginmodule *)pi_mip->priv;
    g_assert(pi_mp);
    // printf("  Postlink: Api=%p iface = %p peer_mip_name=(%s/%s)\n",api,iface,pi_mp->name,pi_mip->name);
  }
  
  return;
}
static void
rwplugin_c_extension_module_instance_preunlink(RwpluginApi *api,
                                               GObject *module_instance_handle,
                                               guint32 linkid,
                                               GObject *iface_provider,
                                               GObject *iface_mih,
                                               guint32 apitype,
                                               const gchar *apistr)
{
  StandardPluginmoduleInstance *mip = (StandardPluginmoduleInstance *)module_instance_handle;
  StandardPluginmodule *mp = (StandardPluginmodule *)mip->priv;
  g_assert(api);
  g_assert(mip);
  g_assert(mp);

  // printf("rwplugin_c_extension_module_instance_preunlink(%s/%s) %u %s\n",
  //       mp->name,mip->name,linkid,apistr);

  if (apitype == STANDARD_PLUGIN_TYPE_API) {
    StandardPluginApi      *api = STANDARD_PLUGIN_API(iface_provider);
    StandardPluginApiIface *iface = STANDARD_PLUGIN_API_GET_INTERFACE(iface_provider);
    g_assert(api);
    g_assert(iface);
    StandardPluginmoduleInstance *pi_mip = (StandardPluginmoduleInstance *)iface_mih;
    g_assert(pi_mip);
    StandardPluginmodule *pi_mp = (StandardPluginmodule *)pi_mip->priv;
    g_assert(pi_mp);
    // printf("  Preunlink: Api=%p iface = %p peer_mip_name=(%s/%s)\n",api,iface,pi_mp->name,pi_mip->name);
  }
  
  return;
}
static void
rwplugin_c_extension_module_instance_postunlink(RwpluginApi *api,
                                                GObject *module_instance_handle,
                                                guint32 linkid,
                                                GObject *iface_provider,
                                                GObject *iface_mih,
                                                guint32 apitype,
                                                const gchar *apistr)
{
  StandardPluginmoduleInstance *mip = (StandardPluginmoduleInstance *)module_instance_handle;
  StandardPluginmodule *mp = (StandardPluginmodule *)mip->priv;
  g_assert(api);
  g_assert(mip);
  g_assert(mp);

  // printf("rwplugin_c_extension_module_instance_postunlink(%s/%s) %u %s\n",
  //        mp->name,mip->name,linkid,apistr);

  if (apitype == STANDARD_PLUGIN_TYPE_API) {
    StandardPluginApi      *api = STANDARD_PLUGIN_API(iface_provider);
    StandardPluginApiIface *iface = STANDARD_PLUGIN_API_GET_INTERFACE(iface_provider);
    g_assert(api);
    g_assert(iface);
    StandardPluginmoduleInstance *pi_mip = (StandardPluginmoduleInstance *)iface_mih;
    g_assert(pi_mip);
    StandardPluginmodule *pi_mp = (StandardPluginmodule *)pi_mip->priv;
    g_assert(pi_mp);
    // printf("  Postunlink: Api=%p iface = %p peer_mip_name=(%s/%s)\n",api,iface,pi_mp->name,pi_mip->name);
  }

  return;
}



static void
rwplugin__api__iface_init(RwpluginApiIface *iface)
{
  // printf("rw_rwplugin_api__iface_init() %p called\n", iface);

  iface->module_init = rwplugin_c_extension_module_init;
  iface->module_deinit = rwplugin_c_extension_module_deinit;

  iface->module_instance_alloc = rwplugin_c_extension_module_instance_alloc;
  iface->module_instance_free = rwplugin_c_extension_module_instance_free;

  iface->module_instance_prelink_check = rwplugin_c_extension_module_instance_prelink_check;
  iface->module_instance_prelink = rwplugin_c_extension_module_instance_prelink;
  iface->module_instance_postlink = rwplugin_c_extension_module_instance_postlink;
  iface->module_instance_preunlink = rwplugin_c_extension_module_instance_preunlink;
  iface->module_instance_postunlink = rwplugin_c_extension_module_instance_postunlink;
}


static gint
standard_plugin__api__add(StandardPluginApi *api, gint a, gint b)
{
  static int print_done = 1;
  if (print_done == 0) {
    printf("====================================\n");
    printf("This is a call to the C plugin add()\n");
    printf("====================================\n");
    print_done = 1;
  }
  return a + b;
}

static void
standard_plugin__api__add3(StandardPluginApi *api, gint a, gint b, gint *c)
{
  static int print_done = 1;
  if (print_done == 0) {
    printf("====================================\n");
    printf("This is a call to the C plugin add3()\n");
    printf("====================================\n");
    print_done = 1;
  }
  *c = a + b;
}

static void
standard_plugin__api__invoke_module(StandardPluginApi *api,
                                    GObject *module_handle)
{
  StandardPluginmodule *mp = (StandardPluginmodule *)module_handle;
  g_assert(api);
  g_assert(mp);

  printf("standard_plugin_c_extension_invoke_module() called for module=%s\n",mp->name);  
}

static void
standard_plugin__api__invoke_module_instance(StandardPluginApi *api,
                                             GObject *module_instance_handle)
{
  StandardPluginmoduleInstance *mip = (StandardPluginmoduleInstance *)module_instance_handle;
  StandardPluginmodule *mp = (StandardPluginmodule *)mip->priv;
  g_assert(api);
  g_assert(mip);
  g_assert(mp);

  // printf("standard_plugin_c_extension_invoke_module_instance() called for (%s/%s)\n",
  //        mp->name,mip->name);  
}

static void
standard_plugin_c_extension_init(StandardPluginCExtension *plugin)
{
  // This is called once for each extension created
  // printf("standard_plugin_c_extension_init() %p called\n",plugin);
}

static void
standard_plugin_c_extension_class_init(StandardPluginCExtensionClass *klass)
{
  // printf("standard_plugin_c_extension_class_init() %p called\n",klass);
}

static void
standard_plugin_c_extension_class_finalize(StandardPluginCExtensionClass *klass)
{
  // printf("standard_plugin_c_extension_class_finalize() %p called\n", klass);
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

/* Don't modify the code below, it is autogenerated */

static void
standard_plugin__api__iface_init(StandardPluginApiIface *iface)
{
  iface->add = standard_plugin__api__add;
  iface->add3 = standard_plugin__api__add3;
  iface->invoke_module = standard_plugin__api__invoke_module;
  iface->invoke_module_instance = standard_plugin__api__invoke_module_instance;
}

G_DEFINE_DYNAMIC_TYPE_EXTENDED(StandardPluginCExtension,
        standard_plugin_c_extension,
        PEAS_TYPE_EXTENSION_BASE,
        0,
        G_IMPLEMENT_INTERFACE_DYNAMIC(RW_COMMON_INTERFACE_TYPE_API,
                rwplugin__api__iface_init)
        G_IMPLEMENT_INTERFACE_DYNAMIC(STANDARD_PLUGIN_TYPE_API,
                standard_plugin__api__iface_init)
        )

G_MODULE_EXPORT void
peas_register_types(PeasObjectModule *module)
{
  standard_plugin_c_extension_register_type(G_TYPE_MODULE(module));
  peas_object_module_register_extension_type(module,
        RW_COMMON_INTERFACE_TYPE_API,
        STANDARD_PLUGIN_C_EXTENSION_TYPE);
  peas_object_module_register_extension_type(module,
        STANDARD_PLUGIN_TYPE_API,
        STANDARD_PLUGIN_C_EXTENSION_TYPE);
}

#endif //VAPI_TO_C_AUTOGEN
