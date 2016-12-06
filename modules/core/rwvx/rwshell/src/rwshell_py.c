
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

#include <libpeas/peas.h>

#include "rwshell-api.h"

rwshell_module_ptr_t rwshell_module_alloc()
{
  rw_status_t status;
  rwshell_module_ptr_t rwshell;

  rwshell = (rwshell_module_ptr_t)malloc(sizeof(struct rwshell_module_s));
  if (!rwshell)
    return NULL;

  bzero(rwshell, sizeof(struct rwshell_module_s));

  rwshell->framework = rw_vx_framework_alloc();
  if (!rwshell->framework)
    goto err;

  rw_vx_require_repository("RwShell", "1.0");

  status = rw_vx_library_open(rwshell->framework, "rwshell-plugin", "", &rwshell->mip);
  if (status != RW_STATUS_SUCCESS)
    goto err;


  /* Perftools related */
  rwshell->pt = peas_engine_create_extension(
      rwshell->framework->engine,
      rwshell->mip->modp->plugin_info,
      RW_SHELL_TYPE_PERFTOOLS,
      NULL);
  if (!rwshell->pt)
    goto err;

  rwshell->pt_cls = RW_SHELL_PERFTOOLS(rwshell->pt);
  rwshell->pt_iface = RW_SHELL_PERFTOOLS_GET_INTERFACE(rwshell->pt);



  /* Crashtools related */
  rwshell->ct = peas_engine_create_extension(
      rwshell->framework->engine,
      rwshell->mip->modp->plugin_info,
      RW_SHELL_TYPE_CRASHTOOLS,
      NULL);
  if (!rwshell->ct)
    goto err;

  rwshell->ct_cls = RW_SHELL_CRASHTOOLS(rwshell->ct);
  rwshell->ct_iface = RW_SHELL_CRASHTOOLS_GET_INTERFACE(rwshell->ct);

  goto done;

err:
  rwshell_module_free(&rwshell);

done:

  return rwshell;
}

void rwshell_module_free(rwshell_module_ptr_t * rwshell)
{
  if ((*rwshell)->pt)
    g_object_unref((*rwshell)->pt);

  if ((*rwshell)->ct)
    g_object_unref((*rwshell)->ct);

  if ((*rwshell)->mip)
    rw_vx_modinst_close((*rwshell)->mip);

  if ((*rwshell)->framework)
    rw_vx_framework_free((*rwshell)->framework);

  free(*rwshell);
  *rwshell = NULL;

  return;
}
