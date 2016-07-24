
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
