/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Author(s): Tim Mortsolf
 * Creation Date: 8/29/2013
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#include "basic_plugin-c.h"

static gint
basic_plugin__api__add(BasicPluginApi *api, gint a, gint b)
{
  return a + b;
}

static void
basic_plugin_c_extension_init(BasicPluginCExtension *plugin)
{
  // This is called once for each extension created
  printf("basic_plugin_c_extension_init() %p called\n",plugin);
}

static void
basic_plugin_c_extension_class_init(BasicPluginCExtensionClass *klass)
{
  printf("basic_plugin_c_extension_class_init() %p called\n",klass);
}


static void
basic_plugin_c_extension_class_finalize(BasicPluginCExtensionClass *klass)
{
  printf("basic_plugin_c_extension_class_finalize() %p called\n", klass);
}

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN
/* Don't modify the code below, it is autogenerated */

#endif //VAPI_TO_C_AUTOGEN
