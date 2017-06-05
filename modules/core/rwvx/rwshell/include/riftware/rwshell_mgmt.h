
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __RWSHELL_MGMT_H__
#define __RWSHELL_MGMT_H__

#include "rwvx.h"
#include "rwvcs.h"
#include <rwdts.h>


#ifdef __cplusplus
extern "C" {
#endif

rw_status_t
rwlog_sysmgmt_dts_registration (
    rwtasklet_info_ptr_t tasklet,
    rwdts_api_t *dts_h);

void* start_background_profiler(
    rwtasklet_info_ptr_t rwtasklet_info,
    uint32_t duration_secs,
    bool use_mainq);

typedef struct rw_profiler_s {
  rwtasklet_info_ptr_t tasklet_info;
  rwsched_dispatch_source_t timer;
  rwsched_dispatch_queue_t rwq;
  uint32_t duration_secs;
  bool use_mainq;
} rw_profiler_t;

#define  RW_BACKGROUND_PROFILING
#define  RW_BACKGROUND_PROFILING_PERIOD (5*60) /* 5 minutes */

extern void *g_rw_profiler_handle;
extern bool g_background_profiler_enabled;
#endif /* __RWSHELL_MGMT_H__*/
