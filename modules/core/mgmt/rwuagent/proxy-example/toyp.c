
/**
 * @file toyp.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 29/09/2015
 * @brief Example toy rwsched/tasklet/rwmsg/rwdts process
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rw_json.h>

#include <rwmsg.h>
#include <rwdts.h>

#include <rw-vnf-base-opdata.ypbc.h>
#include <rw-mgmtagt.ypbc.h>
#include <rw-mgmtagt-dts.pb-c.h>



/* Data, a little structure representing our "task" */
struct toyp_s {
  /* Actual application logic, the "instance" contents in RWVCS
     tasklet plugin terms */
  struct {
    int timerct;
    rwdts_xact_t *xact;
  } toy;

  /* Riftware glue and API handles and so forth. */
  rwtasklet_info_t *rwtasklet_info;
  char *name;
  rwdts_api_t *dts;
};

typedef struct toyp_s toyp_t;

/* Forwards */
static void toyp_spoof_vcs(toyp_t *tp);


/* Example timer callback */
static void toyp_cftimer_callback(rwsched_CFRunLoopTimerRef timer, void *info) {
  toyp_t *tp = (toyp_t *)info;

  tp->toy.timerct++;

  if (tp->toy.timerct > 5) {
    if (rwdts_api_get_state(tp->dts) != RW_DTS_STATE_RUN) {
      fprintf(stderr, "Golly, DTS seems never to have connected?  Bailing out\n");
      fprintf(stderr, "Perhaps RWMSG_BROKER_PORT does not match a local broker's port as found in 'show messaging info'\n");
      exit(1);
    } else {
      if (tp->toy.timerct == 6) {
        fprintf(stderr, "toyp_cftimer_callback going silent, as everything seems to be happy\n");
      }
    }
  } else {
    fprintf(stderr, "toyp_cftimer_callback called, timerct=%d\n", tp->toy.timerct);
  }
}

static void toyp_cftimer_start(toyp_t *tp) {
  /* Make a CF timer that ticks at 1 Hz */
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  double timer_interval = 1.0;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tp->rwtasklet_info->rwsched_tasklet_info);
  rwsched_CFRunLoopTimerRef cftimer;
  cf_context.info = tp;
  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
      tp->rwtasklet_info->rwsched_tasklet_info,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + timer_interval,
      timer_interval,
      0,
      0,
      toyp_cftimer_callback,
      &cf_context );

  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
  rwsched_tasklet_CFRunLoopAddTimer(
      tp->rwtasklet_info->rwsched_tasklet_info,
      runloop,
      cftimer,
      rwsched_instance_CFRunLoopGetMainMode(tp->rwtasklet_info->rwsched_instance));
}

static void toyp_rpc_callback(
  rwdts_xact_t *xact,
  rwdts_xact_status_t *status,
  void *ctx)
{
  toyp_t *tp = (toyp_t*)ctx;

  rwdts_query_result_t *res;
  while ((res = rwdts_xact_query_result(xact, 0))) {
    /* PbCM is a generic value.  Normally this would be a real type. */
    const ProtobufCMessage *pbcm = rwdts_query_result_get_protobuf(res);
    if (pbcm) {
      char *str = NULL;
      rw_json_pbcm_to_json(pbcm, NULL, &str);
      fprintf(stderr, "DTS transaction result: %s\n", str);
      free(str);

      RWPB_M_MSG_DECL_SAFE_CAST(RwMgmtagtDts_output_MgmtAgentDts, rsp, pbcm);
      if (rsp) {
        assert(rsp->pb_request);
        if (rsp->pb_request->error) {
          fprintf(stderr, "Error! %s\n", rsp->pb_request->error);
        }

        if (rsp->pb_request->has_data) {
          ProtobufCMessage* rpc_output =
            protobuf_c_message_unpack(
              /*instance*/NULL,
              RWPB_G_MSG_PBCMD(RwVnfBaseOpdata_RwVnfBaseOpdata_output_ClearData),
              rsp->pb_request->data.len,
              rsp->pb_request->data.data);
          str = NULL;
          rw_json_pbcm_to_json(rpc_output, NULL, &str);
          fprintf(stderr, "output: %s\n", str);
          free(str);
        } else {
          fprintf(stderr, "No output data!\n");
        }
      } else {
        fprintf(stderr, "No response or bad type!\n");
      }
    }
  }

  if (status->xact_done) {
    fprintf(stderr, "Rpc transaction complete\n");
    tp->toy.xact = NULL;
  }
}

static void toyp_uagent_rpc(toyp_t *tp) {
  fprintf(stderr, "Start rpc transaction via Agent\n");

  RWPB_M_MSG_DECL_INIT(RwVnfBaseOpdata_RwClearPortCounters, port_counters);
  char vnf_name[16] = "trafgen\x1f";
  port_counters.vnf_name = &vnf_name[0];
  port_counters.has_vnf_instance = true;
  port_counters.vnf_instance = 0;
  port_counters.has_port_name = true;
  strcpy(port_counters.port_name, "trafgen/2/1\x1f");

  RWPB_M_MSG_DECL_INIT(RwVnfBaseOpdata_input_ClearData, clear);
  clear.port_counters = &port_counters;

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_PbRequest, pbreq);
  pbreq.xpath = (char*)("I,/rwvnfopdata:clear-data");
  pbreq.has_request_type = true;
  pbreq.request_type = RW_MGMTAGT_DTS_PB_REQUEST_TYPE_RPC;
  pbreq.has_data = true;
  pbreq.data.data = protobuf_c_message_serialize(
    NULL/*default_instance*/,
    &clear.base,
    &pbreq.data.len );
  assert(pbreq.data.data);

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_input_MgmtAgentDts, mgmtagt);
  mgmtagt.pb_request = &pbreq;

  tp->toy.xact = rwdts_api_query(
    tp->dts,
    "I,/rw-mgmtagt-dts:mgmt-agent-dts",
    RWDTS_QUERY_RPC,
    RWDTS_FLAG_NONE,
    toyp_rpc_callback,
    (void*)tp,
    &mgmtagt.base);

  protobuf_c_instance_free(NULL/*default_instance*/, pbreq.data.data);
}

/* Logically similar to what would be a tasklet component's
   instance_start function.  That one takes various pointers, of which
   our "tp" is directly equivalent to the "instance" pointer */
static int toyp_start(toyp_t *tp) {
  fprintf(stderr, "Task starting!\n");
  toyp_uagent_rpc(tp); /* payload */

  return 0;
}




/* Drive the DTS task recovery state machine through to completion.
   In conventional RiftWare tasklets, there is much bootstrap
   sequencing logic encompased in this state machine, but for a mere
   client it doesn't do much. */
static void toyp_dts_state_change(rwdts_api_t *dts,
                                  rwdts_state_t dts_state,
                                  void *ctx) {
  toyp_t *tp = (toyp_t*)ctx;
  assert(tp->dts == dts);

  fprintf(stderr, "Task got DTS state change to state %d!\n", (int)dts_state);

  switch (dts_state) {
  case RW_DTS_STATE_INIT:
    rwdts_api_set_state(dts, RW_DTS_STATE_REGN_COMPLETE);
    break;
  case RW_DTS_STATE_CONFIG:                          /* BUG is RW_DTS_STATE_CONFIG_READY in header, but that's not a real decl! */
    rwdts_api_set_state(dts, RW_DTS_STATE_RUN);      /* BUG is RW_DTS_STATE_RUN in header, but that's not a real decl! */

    /* Good to go, kick off the actual application */
    toyp_start(tp);
    break;
  default:
    break;
  }
}


/* Construct a toyp tasklet.  Essentially a VCS plugin instance alloc
   method. */
static int toyp_init(toyp_t *tp) {
  assert(tp);

  memset(tp, 0, sizeof(*tp));

  toyp_spoof_vcs(tp);

  fprintf(stderr, "Beginning Tasklet and DTS runtime initialization in %s...\n", tp->name);
  tp->dts = rwdts_api_new(tp->rwtasklet_info,
                          rw_ypbc_gi__rw_vnf_base_opdata_get_schema(),
                          toyp_dts_state_change,
                          tp,
                          NULL);
  assert(tp->dts);
  rwdts_api_add_ypbc_schema(tp->dts,
                            (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwMgmtagtDts));

  /* Run a timer to check for the thing connecting to the DTS domain
     and complain/exit if that doesn't happen. */
  toyp_cftimer_start(tp);

  return TRUE;
}

/* Similar to a VCS plugin stop method */
static void toyp_deinit(toyp_t *tp) {
  assert(tp);

  rw_status_t rs = rwdts_api_deinit(tp->dts);
  assert(rs == RW_STATUS_SUCCESS);

  int r = rwmsg_endpoint_halt(tp->rwtasklet_info->rwmsg_endpoint);
  assert(r);

  rwsched_tasklet_free(tp->rwtasklet_info->rwsched_tasklet_info);

  rwsched_instance_free(tp->rwtasklet_info->rwsched_instance);

  memset(tp, 0, sizeof(*tp));
}




/*
 *******  Begin duct tape; nothing but ugliness below  ******
 */


int main(int argc, const char **argv, const char **envp) {
  fprintf(stderr, "Main, creating toyp...\n");
  toyp_t tp;
  toyp_init(&tp);

  fprintf(stderr, "Main, running scheduler...\n");
  rwsched_instance_CFRunLoopRunInMode(tp.rwtasklet_info->rwsched_instance,
                                      rwsched_instance_CFRunLoopGetMainMode(tp.rwtasklet_info->rwsched_instance),
                                      INT_MAX,
                                      FALSE);
  fprintf(stderr, "Main, scheduler done (should be impossible)...\n");

  toyp_deinit(&tp);

  exit(0);
}

/* Set up various items to fake tasklet and VCS plugin
   infrastructure. */
static void toyp_spoof_vcs(toyp_t *tp) {

  /* Configure the rwmsg layer broker settings.  Normally there is a
     Yang-defined configuration object passed in with
     rwmsg_endpoint_create().  However to avoid Yang entanglements as
     we bring up this hello world task, we just use environment
     variables and a little magical knowledge to bodge our way
     through. */
  setenv("RWMSG_BROKER_ENABLE", "1", TRUE);
  char *p = getenv("RWMSG_BROKER_PORT");
  if (p) {
    fprintf(stderr, "RWMSG_BROKER_PORT=%s\n", p);
  } else {
    uint16_t port = 0;
    rw_unique_port(21234 /* BASE_BROKER_PORT */, &port);   /* BUG BASE_BROKER_PORT and related logic needs to be in some public API */
    if (port) {
      char tmp[99];
      sprintf(tmp, "%d", (int)port);
      setenv("RWMSG_BROKER_PORT", tmp, TRUE);
      fprintf(stderr, "No RWMSG_BROKER_PORT, rw_unique_port gave port %d\n", (int)port);
    } else {
      fprintf(stderr, "No RWMSG_BROKER_PORT, rw_unique_port failed, rendezvous will be unlikely\n");
    }
  }

  /* Establish a bogus/empty rwlog filterset in lieu of shm filters */
  rwlog_init_bootstrap_filters(NULL);

  /* Make a fake tasklet_info object, same as rwvcs would pass into tasklet's start function */
  tp->rwtasklet_info = RW_MALLOC0(sizeof(*tp->rwtasklet_info));
  tp->rwtasklet_info->ref_cnt = 1;
  tp->rwtasklet_info->rwsched_instance = rwsched_instance_new();
  tp->rwtasklet_info->rwsched_tasklet_info = rwsched_tasklet_new(tp->rwtasklet_info->rwsched_instance);
  tp->rwtasklet_info->rwmsg_endpoint = rwmsg_endpoint_create(1, 0, 0, tp->rwtasklet_info->rwsched_instance, tp->rwtasklet_info->rwsched_tasklet_info, rwtrace_init(), NULL);
  asprintf(&tp->name, "toyp-%u", getpid());
  tp->rwtasklet_info->rwlog_instance = rwlog_init(tp->name);
  tp->rwtasklet_info->identity.rwtasklet_name = "toyp";
  tp->rwtasklet_info->identity.rwtasklet_instance_id = getpid();
}
