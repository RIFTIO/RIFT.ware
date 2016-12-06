/*
 * Copyright (c) 2014 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OVS_TNL_ROUTER_H
#define OVS_TNL_ROUTER_H 1

#include "util.h"

#ifdef  __cplusplus
extern "C" {
#endif

bool ovs_router_lookup(ovs_be32 ip_dst, char out_dev[], ovs_be32 *gw);
void ovs_router_init(void);
void ovs_router_insert(ovs_be32 ip_dst, uint8_t plen,
                       const char output_bridge[], ovs_be32 gw);
void ovs_router_flush(void);
#ifdef  __cplusplus
}
#endif

#endif
