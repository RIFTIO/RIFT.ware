/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file redis_client.h
 * @author Prashanth Bhaskar
 * @date 06/10/2016
 */

#ifndef __REDIS_CLIENT_H__
#define __REDIS_CLIENT_H__


#include "rwmain.h"
#include <dlfcn.h>

__BEGIN_DECLS

rw_status_t
rwmain_gen_redis_conf_file(struct rwmain_gi * rwmain,
                           vcs_manifest_component *m_component);

void rwmain_start_redis_client(struct rwmain_gi * rwmain);

void 
rwmain_redis_notify_transition(struct rwmain_gi *rwmain, vcs_vm_state state);
__END_DECLS

#endif // __REDIS_CLIENT_H__
