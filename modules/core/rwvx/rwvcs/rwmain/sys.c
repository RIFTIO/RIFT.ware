
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

#include <dirent.h>
#include <stdio.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "rwmain.h"

static void update_proc_cputime(struct rwmain_gi * rwmain)
{
  int r;
  struct timespec cputime;
  struct timespec systime;
  double sys;
  double cpu;

  /* Get the cpu time being using by this process */
  r = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cputime);
  RW_ASSERT(r == 0);

  r = clock_gettime(CLOCK_MONOTONIC_COARSE, &systime);
  RW_ASSERT(r == 0);

  sys = rwmain->sys.last_proc_systime;
  cpu = rwmain->sys.last_proc_cputime;

  rwmain->sys.last_proc_systime = systime.tv_sec + (double)systime.tv_nsec / 1e9;
  rwmain->sys.last_proc_cputime = cputime.tv_sec + (double)cputime.tv_nsec / 1e9;

  rwmain->sys.proc_cpu_usage = 100.0 * ((rwmain->sys.last_proc_cputime - cpu) / (rwmain->sys.last_proc_systime - sys));
  if (lrint(rwmain->sys.proc_cpu_usage) >= 100)
    rwmain->sys.proc_cpu_usage = 100.0;
}

static void update_sys_cputime(struct rwmain_gi * rwmain)
{
  FILE * fp;
  char * line = NULL, cpuname[128];
  ssize_t line_len = 0;
  unsigned long cpu_stats[10];
  size_t cline_len;
  int cpu_i = 0, i;

  fp = fopen("/proc/stat", "r");
  if (!fp) {
    perror("fopen:");
    RW_CRASH();
  }

  line_len = getline(&line, &cline_len, fp);
  if (line_len < 0)
    perror("getline:");
  RW_ASSERT(line_len > 0);

  while (!strncmp(line, "cpu", 3) && (cpu_i < RWVCS_MAX_CPU)) {
    unsigned long total = 0;
    unsigned long n_ticks;
    struct cpu_usage * cpu_usage;

    /* See man(5) proc */
    memset(cpu_stats, 0, sizeof(cpu_stats));
    int r = sscanf(
        line,
        "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
        cpuname,
        &cpu_stats[0],
        &cpu_stats[1],
        &cpu_stats[2],
        &cpu_stats[3],
        &cpu_stats[4],
        &cpu_stats[5],
        &cpu_stats[6],
        &cpu_stats[7],
        &cpu_stats[8],
        &cpu_stats[9]);
    RW_ASSERT(r > 1);
    r--;
    if (cpu_i == 0)
      cpu_usage = &rwmain->sys.all_cpus;
    else
      cpu_usage = rwmain->sys.each_cpu[cpu_i - 1];

    // ncpus changed?
    RW_ASSERT(cpu_usage);

    for (i = 0; i < r; i++) {
      total += cpu_stats[i];
    }
    n_ticks = total - cpu_usage->previous.total;

    cpu_usage->current.user = 100.0 * (double)(cpu_stats[0] - cpu_usage->previous.user)/ n_ticks;
    cpu_usage->current.sys = 100.0 * (double)(cpu_stats[2] - cpu_usage->previous.sys)/ n_ticks;
    cpu_usage->current.idle = 100.0 * (double)(cpu_stats[3] - cpu_usage->previous.idle)/ n_ticks;

    cpu_usage->previous.user = cpu_stats[0];
    cpu_usage->previous.sys = cpu_stats[2];
    cpu_usage->previous.idle = cpu_stats[3];
    cpu_usage->previous.total = total;

    if (lrint(cpu_usage->current.user) >= 100)
      cpu_usage->current.user = 100.0;

    if (lrint(cpu_usage->current.sys) >= 100)
      cpu_usage->current.sys = 100.0;

    if (lrint(cpu_usage->current.idle ) >= 100)
      cpu_usage->current.idle = 100.0;

    cpu_i++;

    free(line);
    line = NULL;
    line_len = getline(&line, &cline_len, fp);
    if (line_len < 0)
      perror("getline:");
    RW_ASSERT(line_len > 0);
  }

  if (line)
    free(line);

  fclose(fp);
}

/*
 * Update the current cpu utilization over the last time delta
 *
 * @param timer - timer executing this callback
 * @param info  - user data
 */
static void on_update_cputime(rwsched_CFRunLoopTimerRef timer, void * info)
{
  struct rwmain_gi * rwmain;

  UNUSED(timer);

  rwmain = (struct rwmain_gi *)info;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);

  update_proc_cputime(rwmain);
  update_sys_cputime(rwmain);
}


void rwmain_setup_cputime_monitor(struct rwmain_gi * rwmain)
{
  FILE * fp;
  char * line = NULL;
  ssize_t line_len = 0;
  size_t cline_len;
  int n_cpus;
  rwsched_CFRunLoopTimerRef cftimer;
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };

  cf_context.info = rwmain;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
      rwmain->tasklet_info->rwsched_tasklet_info,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent(),
      2.0,
      0,
      0,
      on_update_cputime,
      &cf_context);

  rwsched_tasklet_CFRunLoopAddTimer(
      rwmain->tasklet_info->rwsched_tasklet_info,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->tasklet_info->rwsched_tasklet_info),
      cftimer,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);

  fp = fopen("/proc/stat", "r");
  if (!fp) {
    perror("fopen(\"/proc/stat\")");
    return;
  }

  line_len = getline(&line, &cline_len, fp);
  if (line_len < 0)
    perror("getline:");
  RW_ASSERT(line_len > 0);

  n_cpus = 0;
  while (!strncmp(line, "cpu", 3) && n_cpus < RWVCS_MAX_CPU) {
    n_cpus += 1;

    free(line);
    line = NULL;

    line_len = getline(&line, &cline_len, fp);
    if (line_len < 0)
      perror("getline:");
    RW_ASSERT(line_len > 0);
  }

  fclose(fp);
  if (line)
    free(line);


  rwmain->sys.each_cpu = (struct cpu_usage **)malloc(sizeof(struct cpu_usage *) * n_cpus);
  for (int i = 0; i < n_cpus - 1; i++) {
    rwmain->sys.each_cpu[i] = (struct cpu_usage *)malloc(sizeof(struct cpu_usage));
    bzero(rwmain->sys.each_cpu[i], sizeof(struct cpu_usage));
  }
  rwmain->sys.each_cpu[n_cpus - 1] = NULL;

}


