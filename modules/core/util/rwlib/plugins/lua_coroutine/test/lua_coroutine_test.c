/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Author(s): Tim Mortsolf
 * Creation Date: 8/29/2013
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>

#include <glib.h>
#include <libpeas/peas.h>

#include "lua_coroutine.h"

static void
test_lua_coroutine_interface(PeasEngine *engine,
		                        PeasPluginInfo *info)
{
  PeasExtension *extension;
  LuaCoroutineApi *api;
  LuaCoroutineApiIface *interface;
  gint a, b, result;
  LuaCoroutineFunctor *functor = (LuaCoroutineFunctor *)NULL;

  // Create a peas extension for the API interface.
  // The GType for API interface is LUA_COROUTINE_TYPE_API.
  extension = peas_engine_create_extension(engine, 
                                           info,
					                                 LUA_COROUTINE_TYPE_API,
					                                 NULL);
  g_assert(PEAS_IS_EXTENSION(extension));
  g_assert(LUA_COROUTINE_IS_API(extension));

  api = LUA_COROUTINE_API(extension);
  interface = LUA_COROUTINE_API_GET_INTERFACE(extension);

  // Set the input parmaeters for add(a, b)
  a = 1;
  b = 2;

  // Call the plugin function using the function pointer
  result = interface->add_coroutine_create(api, a, b, &functor);
  g_assert_cmpint(result, ==, a);

  // Destroy the created peas extension
  g_object_unref(extension);
}

/**
 * This function creates the peas engine
 */
PeasEngine *create_peas_engine(char *repo_dirs[],
                               char *plugin_dirs[],
                               char *plugin_name,
                               char *plugin_version)
{
  GError *error = NULL;
  int i;
  PeasEngine *engine = NULL;

  // setup repository search path, this is where the typelib files should
  // exist for python 
  for (i = 0; repo_dirs[i]; ++i) {
    printf("repo_dirs = %s\n", repo_dirs[i]);
    g_irepository_prepend_search_path(repo_dirs[i]);
  }

  // Load the repositories
  g_irepository_require(g_irepository_get_default(), "Peas", "1.0", 0, &error);
  g_assert_no_error(error);

  g_irepository_require(g_irepository_get_default(), 
                        plugin_name, plugin_version, 0, &error);
  g_assert_no_error(error);

  // Initialize a PEAS engine
  engine = peas_engine_new();
  g_object_add_weak_pointer(G_OBJECT(engine), (gpointer *) &engine);

  // Inform the PEAS engine where the plugins reside 
  for (i = 0; plugin_dirs[i]; ++i) {
    printf("plugin_dirs = %s\n", plugin_dirs[i]);
    peas_engine_add_search_path(engine, plugin_dirs[i], plugin_dirs[i]);
  }

  // Enable python loader
  peas_engine_enable_loader(engine, "lua5.1");

  return engine;
}

#define NUM_1K_BLOCKS 1024 * 512
// #define NUM_1K_BLOCKS 1024

int
main(int argc, char **argv)
{
  PeasEngine *engine = NULL;
  PeasPluginInfo *info = NULL;
  int i;

  char *repo_dirs[] = { "/usr/lib64/girepository-1.0", PLUGINDIR, NULL };
  char *plugin_dirs [] = { PLUGINDIR, NULL };
  engine = create_peas_engine(repo_dirs, plugin_dirs, "LuaCoroutine", "1.0");


  /* 
   * This block is to simulate the issue with LUAJIT and 64 bit pointers
   * Allocating large amount of memory before initailizing the LUA state
   * causes an issue.
   */
  for (i = 0; i < NUM_1K_BLOCKS; i++) {    /* this will fail for lua */
    char *m = (char *)malloc(4096);
    g_assert(m);
    if (!m) {
       printf("not able to allocate memory\n");
    }
  }

  info = peas_engine_get_plugin_info(engine, "lua_coroutine-lua");
  peas_engine_load_plugin(engine, info);
  test_lua_coroutine_interface(engine, info);

  return 0;
}
