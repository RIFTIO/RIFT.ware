
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwshell-api.h
 * @author Rajesh Ramankutty
 * @date 2015/05/08
 * @brief Top level API include for rwshell submodule
 */

#ifndef __REPERF_API_H__
#define __REPERF_API_H__

#include <stdbool.h>

#include <libpeas/peas.h>

#include <rwshell.h>

#include <rwlib.h>
#include <rw-manifest.pb-c.h>
#include <rw_vx_plugin.h>

__BEGIN_DECLS

struct rwshell_module_s {
  rw_vx_framework_t * framework;
  rw_vx_modinst_common_t *mip;

  PeasExtension * pt;
  RwShellPerftools * pt_cls;
  RwShellPerftoolsIface * pt_iface;

  PeasExtension * ct;
  RwShellCrashtools * ct_cls;
  RwShellCrashtoolsIface * ct_iface;
};
typedef struct rwshell_module_s * rwshell_module_ptr_t;

/*
 * Allocate a rwshell module.  Once allocated, the clients within
 * the module still need to be initialized.
 * It is a fatal error to attempt to use any
 * client before it has been initialized.  However, it is
 * perfectly fine to not initialize a client that will remain
 * unused.  Note that every function contains the client that it
 * will use as part of the name, just after the rwshell_ prefix.
 *
 * @return - rwshell module handle or NULL on failure.
 */
rwshell_module_ptr_t rwshell_module_alloc();

/*
 * Deallocate a rwshell module.
 *
 * @param - pointer to the rwshell module to be deallocated.
 */
void rwshell_module_free(rwshell_module_ptr_t * rwshell);

rw_status_t rwshell_perf_start(
    rwshell_module_ptr_t rwshell,
    const char **pids,
    unsigned int frequency,
    const char *perf_data,
    char **command,
    int *newpid);

rw_status_t rwshell_perf_stop(
    rwshell_module_ptr_t rwshell,
    int newpid,
    char **command);

rw_status_t rwshell_perf_report(
    rwshell_module_ptr_t rwshell,
    double percent_limit,
    const char *perf_data,
    char **command,
    char **report);



rw_status_t rwshell_crash_report(
    rwshell_module_ptr_t rwshell,
    const char *vm_name,
    char ***crashes);

__END_DECLS

#endif


