
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

/*
 * rwmsgbroker-c.h
 */

#ifndef __RWMSGBROKER_C_H__
#define __RWMSGBROKER_C_H__

#include "rwmsg_int.h"
#include "rwmsg_broker.h"
#include "rwmsg_broker_tasklet.h"

#include <libpeas/peas.h>

#define RWMSGBROKER_TYPE_COMPONENT RW_TASKLET_PLUGIN_TYPE_COMPONENT
typedef RwTaskletPluginComponentIface RwmsgbrokerComponentIface;

#define VAPI_TO_C_AUTOGEN
#ifdef VAPI_TO_C_AUTOGEN

#endif //VAPI_TO_C_AUTOGEN

#endif /* __RWMSGBROKER_C_H__ */