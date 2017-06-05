/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Author(s): Anil Gunturu
 * Creation Date: 04/10/2014
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#include <stdio.h>

#include "rwlib.h"
#include "rw_vx_plugin.h"
#include "../vala/rwcli_plugin.h"

#if !defined(INSTALLDIR) || !defined(PLUGINDIR)
#error - Makefile must define INSTALLDIR and PLUGINDIR
#endif

/**
 * This code does basic testing of rwcli_plugin.
 * It enures that the plugin loads properly
 *
 * rwcli_plugin provides a python plugin for invoking
 * cli print hooks.
 *
 * This code uses the rw_vx api, instead of directly calling
 * peas APIs
 */

int main(int argc, char **argv)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rw_vx_modinst_common_t *common = NULL;
  rw_vx_framework_t *vxfp;
  rwcli_pluginPrint *klass;
  rwcli_pluginPrintIface *iface;

  rw_vx_prepend_gi_search_path(PLUGINDIR);
  rw_vx_prepend_gi_search_path("../../vala");

  rw_vx_require_repository("rwcli_plugin", "1.0");

  /*
   * Allocate a RIFT_VX framework to run the test
   * NOTE: this initializes a LIBPEAS instance
   */
  vxfp = rw_vx_framework_alloc();
  RW_ASSERT(vxfp);

  rw_vx_add_peas_search_path(vxfp, PLUGINDIR);

  // Register the plugin
  status = rw_vx_library_open(vxfp,
                              "rwcli_plugin",
                              (char *)"",
                              &common);

  RW_ASSERT((RW_STATUS_SUCCESS == status) && (NULL != common));

  // Get the rwcli_plugin api and interface pointers
  status = rw_vx_library_get_api_and_iface(common,
                                           RWCLI_PLUGIN_TYPE_PRINT,
                                           (void **) &klass,
                                           (void **) &iface,
                                           NULL);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  RW_ASSERT(klass);
  RW_ASSERT(iface);

  return 0;
}
