
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
 * @file rwmsg_links.c
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Do nothing test program to validate declarations, linkage, compile-time stuff
 */

#include <stdlib.h>

#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwmsg.h>

#include <rwdts.h>
#include <rwdts_router.h>

RwSchemaPathSpec banana;

int main(int argc, char **argv, char **envp) {
  rw_schema_path_spec__init(&banana);
  banana.strpath = (char*)"banana";
  printf("Banana\n");
  exit(0);

  RWDtsXact xmsg;
  rwdts_xact__init(&xmsg);
  rwdts_query_router__execute(NULL, NULL, &xmsg, NULL, NULL);

  rwdts_router_t *dts = rwdts_router_init(NULL, NULL, NULL, NULL, NULL, 0);
  rwdts_router_deinit(dts);
  dts = NULL;
}
