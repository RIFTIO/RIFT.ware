
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
