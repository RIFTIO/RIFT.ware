
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <string.h>

#include "rwshell-api.h"

rw_status_t rwshell_perf_start(
    rwshell_module_ptr_t rwshell,
    const char **pids,
    unsigned int frequency,
    const char *perf_data,
    char **command,
    int *newpid)
{
  rw_status_t status;

  status = rwshell->pt_iface->start_perf(
      rwshell->pt_cls,
      (gchar **)pids,
      frequency,
      (gchar *)perf_data,
      (gchar **)command,
      newpid);

  return status;
}

rw_status_t rwshell_perf_stop(
    rwshell_module_ptr_t rwshell,
    int newpid,
    char **command)
{
  rw_status_t status;

  status = rwshell->pt_iface->stop_perf(
      rwshell->pt_cls,
      newpid,
      (gchar **)command);

  return status;
}

rw_status_t rwshell_perf_report(
    rwshell_module_ptr_t rwshell,
    double percent_limit,
    const char *perf_data,
    char **command,
    char **report)
{
  rw_status_t status;

  status = rwshell->pt_iface->report_perf(
      rwshell->pt_cls,
      percent_limit,
      (gchar *)perf_data,
      command,
      report);

  //printf("%s", report);

  return status;
}
