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
 * Author(s): Anil Gunturu
 * Creation Date: 04/10/2014
 * 
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
