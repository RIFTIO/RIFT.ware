
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <string.h>

#include "rwshell-api.h"

rw_status_t rwshell_crash_report(
    rwshell_module_ptr_t rwshell,
    const char *vm_name,
    char ***crashes)
{
  rw_status_t status;

  status = rwshell->ct_iface->report_crash(
      rwshell->ct_cls,
      vm_name,
      crashes);

  //printf("%s", report);

  return status;
}
