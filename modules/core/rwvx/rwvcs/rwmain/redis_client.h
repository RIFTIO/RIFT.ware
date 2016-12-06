
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
 */


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
