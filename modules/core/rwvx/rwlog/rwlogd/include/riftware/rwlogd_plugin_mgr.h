
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwlogd_plugin_mgr.h
 * @author Sameer Nayak (Sameer.Nayak@riftio.com)
 * @date 29/01/2015
 * @brief How to add Plugins? "Refer rwlogd_cli_sink and rwlogd_syslog_sink "
**/

#ifndef __RWLOGD_PLUGIN_MGR_H__
#define __RWLOGD_PLUGIN_MGR_H__
#include "rwlogd.h"
/*! 
 * rwlogd_create_sink_dynamic : Method
 *
 * Shared library name will be resolved as =>  "lib" + library_name + ".so"
 *
 * Start Function will be resolved as => "start_" + library_name
 *
 * Startfunction will be called as 
 * "start_" + library_name (inst, name, host, port)
 *
 * @@name => Name of the Sink. For multiplexing and future references
 *
 * @@host => Name of the destination to be connected 
             Eg: Rw.Cli or Syslog

 * @@port => instance of the destination
 *           Eg: 1 ,2,.. for CLI 
 *           514, .. for syslog
 *
 * Refer rwlogd_cli_sink and rwlogd_syslog_sink 
 *
 * TBD: Automagically generating basic sink code. Similar to component code
 * Will be taken up for the next sink 
 *
 */
rw_status_t rwlogd_create_sink_dynamic(const char *library_name,
                                   rwlogd_instance_ptr_t dat,
                                   const char *name,
                                   const char *host,
                                   const  int port);
#define rwlogd_create_sink(a, b, c , d, e) rwlogd_create_sink_dynamic(#a, b, c , d, e)

/*! 
 * rwlogd_delete_sink_dynamic : Method
 *
 * Shared library name will be resolved as =>  "lib" + library_name + ".so"
 *
 * Stop Function will be resolved as => "st_" + library_name
 *
 * Stopfunction will be called as 
 * "stop_" + library_name (inst, name, host, port)
 *
 * @@name => Name of the Sink. For multiplexing and future references
 *
 * @@host => Name of the destination to be connected 
             Eg: Rw.Cli or Syslog

 * @@port => instance of the destination
 *           Eg: 1 ,2,.. for CLI 
 *           514, .. for syslog
 *
 */
rw_status_t rwlogd_delete_sink_dynamic (const char *library_name,
                                   rwlogd_instance_ptr_t inst, const char *name,
                                   const char *host,
                                   const  int port);
#define rwlogd_delete_sink(a, b, c , d, e) rwlogd_delete_sink_dynamic(#a, b, c , d, e)
#endif
