
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


#ifndef __RWMSG_GTEST_C_H
#define __RWMSG_GTEST_C_H

__BEGIN_DECLS

int rwmsg_gtest_queue_create_delete(rwmsg_endpoint_t *ep);
int rwmsg_gtest_queue_queue_dequeue(rwmsg_endpoint_t *ep);
int rwmsg_gtest_queue_pollable_event(rwmsg_endpoint_t *ep);
int rwmsg_gtest_srvq(rwmsg_srvchan_t *sc);

__END_DECLS

#endif // __RWMSG_GTEST_C_H
