
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <glib.h>
#include <libpeas/peas.h>

#include "standard_plugin.h"

GPtrArray *log_hooks = NULL;

static void log_handler(const gchar *log_domain,
	                      GLogLevelFlags log_level,
	                      const gchar *message,
	                      gpointer user_data)
{
  (void)(log_domain);
  (void)(log_level);
  g_log_default_handler(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, message, user_data);
}

static void
gnome_library_init(char *progname)
{
#if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
#endif

  g_set_prgname(progname);

  // Don't abort on warnings or criticals
  g_log_set_always_fatal(G_LOG_LEVEL_ERROR);
  g_log_set_default_handler(log_handler, NULL);
  g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
}

static void
test_extension_call_module_init(PeasEngine     *engine,
                                PeasPluginInfo *info)
{
  PeasExtension *extension;
  StandardPluginApi *standard_api;
  RwpluginApi *common_api;
  StandardPluginApiIface *standard_interface;
  RwpluginApiIface *common_interface;
  gchar *module_name1, *module_name2;
  gchar *module1_alloc_cfg, *module2_alloc_cfg;
  GObject *module1 = NULL, *module2 = NULL;
  gint rc;

  module_name1 = g_strdup("module-name-1");
  g_assert(module_name1);
  module1_alloc_cfg = g_strdup("alloc_cfg_str1");
  g_assert(module1_alloc_cfg);
  module_name2 = g_strdup("module-name-2");
  g_assert(module_name2);
  module2_alloc_cfg = g_strdup("alloc_cfg_str2");
  g_assert(module2_alloc_cfg);
  
  // Create an instance of the STANDARD_PLUGIN_API class
  extension = peas_engine_create_extension(engine, info,
					   STANDARD_PLUGIN_TYPE_API,
					   NULL);
  g_assert(PEAS_IS_EXTENSION(extension));
  g_assert(STANDARD_PLUGIN_IS_API(extension)); // Supports this API?

  standard_api = STANDARD_PLUGIN_API(extension);
  g_assert(standard_api);
  standard_interface = STANDARD_PLUGIN_API_GET_INTERFACE(extension);
  g_assert(standard_interface);

  common_api = RWPLUGIN_API(extension);
  g_assert(common_api);
  common_interface = RWPLUGIN_API_GET_INTERFACE(extension);
  g_assert(common_interface);

  // Call the plugin function using the method name "module_init"
  rc = peas_extension_call(extension, "module_init",module_name1,module1_alloc_cfg,&module1);
  g_assert(rc == 1);
  g_assert(NULL != module1);
  // Call the plugin function using the method name "module_deinit"
  rc = peas_extension_call(extension, "module_deinit",module1);
  g_assert(rc == 1);

  // Call the plugin function using the function pointer
  module2 = common_interface->module_init(common_api,module_name2,module2_alloc_cfg);
  g_assert(module2);
  common_interface->module_deinit(common_api,module2);

  // string cleanup
  g_free(module_name1);
  g_free(module_name2);
  g_free(module1_alloc_cfg);
  g_free(module2_alloc_cfg);
  
  // Destroy the created STANDARD_PLUGIN_API class
  g_object_unref(extension);
}

static void
test_extension_call_add(PeasEngine     *engine,
			PeasPluginInfo *info)
{
  PeasExtension *extension;
  StandardPluginApi *api;
  StandardPluginApiIface *interface;
  gint a, b, result;

  // Create an instance of the STANDARD_PLUGIN_API class
  extension = peas_engine_create_extension(engine, info,
					   STANDARD_PLUGIN_TYPE_API,
					   NULL);
  g_assert(PEAS_IS_EXTENSION(extension));
  g_assert(STANDARD_PLUGIN_IS_API(extension));  // Supports this API?

  api = STANDARD_PLUGIN_API(extension);
  interface = STANDARD_PLUGIN_API_GET_INTERFACE(extension);

  // Set the input parmaeters for add(a, b)
  a = 1;
  b = 2;
  result = 0;

  // Call the plugin function using the method name "add"
  peas_extension_call(extension, "add", a, b, &result);
  g_assert_cmpint(result, ==, a + b);

  // Call the plugin function using the function pointer
  result = interface->add(api, a, b);
  printf("result of interface->add(%d, %d) = %d\n", a, b, result);
  g_assert_cmpint(result, ==, a + b);

  // Call the plugin function add3() using the function pointer
  result = 99;
  interface->add3(api, a, b, &result);
  printf("result of interface->add3(%d, %d) = %d\n", a, b, result);
  g_assert_cmpint(result, ==, a + b);

  // Destroy the created STANDARD_PLUGIN_API class
  g_object_unref(extension);
}

static void
test_extension_call_create_module_instances(PeasEngine     *engine,
                                            PeasPluginInfo *info)
{
  PeasExtension *extension;
  StandardPluginApi *standard_api;
  RwpluginApi *common_api;
  StandardPluginApiIface *standard_interface;
  RwpluginApiIface *common_interface;
  GObject *module1, *module2;
  GObject *module_instance1, *module_instance2;
  gchar *module_name1, *module_name2, *module_instance_name1, *module_instance_name2;
  gchar *module_alloc_cfg_str, *mi_alloc_cfg_str;

  printf("\nCalling: test_extension_call_create_module_instances()\n");
  
  // Create an instance of the STANDARD_PLUGIN_API class
  extension = peas_engine_create_extension(engine, info,
					   STANDARD_PLUGIN_TYPE_API,
					   NULL);
  g_assert(PEAS_IS_EXTENSION(extension));
  g_assert(STANDARD_PLUGIN_IS_API(extension));

  common_api = RWPLUGIN_API(extension);
  g_assert(common_api);
  common_interface = RWPLUGIN_API_GET_INTERFACE(extension);
  g_assert(common_interface);
  
  standard_api = STANDARD_PLUGIN_API(extension);
  g_assert(standard_api);
  standard_interface = STANDARD_PLUGIN_API_GET_INTERFACE(extension);
  g_assert(standard_interface);

  module_name1 = g_strdup("module-name-1");
  g_assert(module_name1);
  module_name2 = g_strdup("module-name-2");
  g_assert(module_name2);
  module_instance_name1 = g_strdup("mi-1");
  g_assert(module_instance_name1);
  module_instance_name2 = g_strdup("mi-2");
  g_assert(module_instance_name2);
  module_alloc_cfg_str = g_strdup("");
  g_assert(module_alloc_cfg_str);
  mi_alloc_cfg_str = g_strdup("");
  g_assert(mi_alloc_cfg_str);

  // Create two module and get an opaque handle from them
  module1 = common_interface->module_init(common_api,module_name1,module_alloc_cfg_str);
  g_assert(module1);
  module2 = common_interface->module_init(common_api,module_name2,module_alloc_cfg_str);
  g_assert(module2);

  // Create two module instances and get an opaque handle from them
  module_instance1 = common_interface->module_instance_alloc(common_api,module1,module_instance_name1,mi_alloc_cfg_str);
  g_assert(module_instance1);
  module_instance2 = common_interface->module_instance_alloc(common_api,module2,module_instance_name2,mi_alloc_cfg_str);
  g_assert(module_instance2);

  // Invoke on the modules
  standard_interface->invoke_module(standard_api, module1);
  standard_interface->invoke_module(standard_api, module2);

  // Invoke on the module instances
  standard_interface->invoke_module_instance(standard_api, module_instance1);
  standard_interface->invoke_module_instance(standard_api, module_instance2);

  // Free the two module instances
  common_interface->module_instance_free(common_api,module_instance1);
  common_interface->module_instance_free(common_api,module_instance2);

  // Free the two modules
  common_interface->module_deinit(common_api,module1);
  common_interface->module_deinit(common_api,module2);

  // string cleanup
  g_free(module_name1);
  g_free(module_name2);
  g_free(module_instance_name1);
  g_free(module_instance_name2);
  g_free(module_alloc_cfg_str);
  g_free(mi_alloc_cfg_str);
  
  // Destroy the created STANDARD_PLUGIN_API class
  g_object_unref(extension);
}

const gchar *allowed_patterns[] = {
  "*lib*loader.so*cannot open shared object file: No such file or directory*"
};

int main(int argc, char **argv)
{
  GError *error = NULL;
  PeasEngine *engine = NULL;
  PeasPluginInfo *info = NULL;
  gchar *extension_plugin = NULL;
  const char *extension_list[] = { "c", "python", NULL };
  char **extension = (char **) extension_list;
  const char *install_dir;
  const char *plugin_dir;

  // get the command line arguments
  if (argc != 3) {
    printf("Usage: %s <installdir> <plugindir>\n", argv[0]);
    exit(1);
  }
  install_dir = argv[1];
  plugin_dir = argv[2];

  // Intialize the Gnome libraries
  gnome_library_init("test_plugin");

  // Setup the repository paths
  g_irepository_prepend_search_path("/usr/lib64/girepository-1.0");
  g_irepository_prepend_search_path(plugin_dir);
  g_irepository_prepend_search_path("../rwplugin/vala");

  // Load the repositories
  g_irepository_require(g_irepository_get_default(), "Peas", "1.0", 0, &error);
  g_assert_no_error(error);
  g_irepository_require(g_irepository_get_default(), "Rwplugin", "1.0", 0, &error);
  g_assert_no_error(error);
  g_irepository_require(g_irepository_get_default(), "StandardPlugin", "1.0", 0, &error);
  g_assert_no_error(error);

  // Initialize a PEAS engine
  engine = peas_engine_new();
  g_object_add_weak_pointer(G_OBJECT(engine), (gpointer *) &engine);
  engine = peas_engine_get_default();

  // Inform the PEAS engine where the plugins reside
  peas_engine_add_search_path(engine, plugin_dir, plugin_dir);

  // Enable every possible language loader
  peas_engine_enable_loader(engine, "python");
  peas_engine_enable_loader(engine, "python3");
  peas_engine_enable_loader(engine, "js");
  peas_engine_enable_loader(engine, "gjs");
  peas_engine_enable_loader(engine, "seed");

  // Run a testcase for each possible extension type
  extension = (char **) extension_list;
  while (*extension) {
    printf("[BEGIN TEST OF EXTENSION '%s']\n",*extension);
    extension_plugin = g_strdup_printf("standard_plugin-%s", *extension);
    info = peas_engine_get_plugin_info(engine, extension_plugin);
    peas_engine_load_plugin(engine, info);
    test_extension_call_add(engine, info);
    test_extension_call_module_init(engine, info);
    test_extension_call_create_module_instances(engine, info);
    printf("[END TEST OF EXTENSION '%s']\n\n",*extension);
    extension++;
  }

  return 0;
}
