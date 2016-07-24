
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwmsg_gtest.cc
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 11/18/2013
 * @brief Google test cases for testing rwmsg
 * 
 * @details Google test cases for testing rwmsg. Specifically
 * this file includes test cases for lotsa stuff(tm).
 */

/** 
 * Step 1: Include the necessary header files 
 */

#include <rwut.h>
#include <vehicle.pb-c.h>

#include "../src/rwuagent.hpp"

using namespace rw_uagent;
using namespace rw_yang;

#if 0
/* Set env variable V=1 and TESTPRN("foo", ...") will be go to stderr */
#define TSTPRN(args...) {                        \
  if (!venv_g.checked) {                        \
    const char *e = getenv("V");                \
    if (e && atoi(e) == 1) {                        \
      venv_g.val = 1;                                \
    }                                                \
    venv_g.checked = 1;                                \
  }                                                \
  if (venv_g.val) {                                \
    fprintf(stderr, args);                        \
  }                                                \
}

static struct {
  int checked;
  int val;
} venv_g;

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;

TEST(RwUagent, NOP) {
  TEST_DESCRIPTION("Tests nothing whatsoever");
}

/* A slight abuse of "unit" testing, to permit uagent testing in a
   simplified multi-tasklet environment. */

typedef struct member_data_ {
  rw_keyspec_path_t *keyspec;
  const ProtobufCMessageDescriptor *desc;
  const ProtobufCMessage *pb;
  rwdts_member_event_cb_t cb;
  rw_keyspec_path_t *resp_keyspec;
  uint32_t flags;
  void *ti;
  void *test;
  //  enum treatment_e ma;
} member_data;

typedef enum {
  TASKLET_ROUTER=0,
  TASKLET_BROKER,
  TASKLET_UAGENT,
#define TASKLET_IS_MEMBER(t) ((t) >= TASKLET_MEMBER_0)
  TASKLET_MEMBER_0,
  TASKLET_MEMBER_1,
  TASKLET_MEMBER_2,
#define TASKLET_MEMBER_COUNT 3  
  TASKLET_CT
} twhich_t;
struct tenv1 {
  rwsched_instance_ptr_t rwsched;
  void *ctxt;
  struct tenv_info {
    char rwmsgpath[1024];
    rwsched_tasklet_ptr_t tasklet;
    rwmsg_endpoint_t *ep;
    rwtasklet_info_t rwtasklet_info;
    // each tasklet might have one or more of:
    void *dts;
    rwdts_api_t *apih;
    void *bro;
    rw_uagent::Instance *uagent;
    member_data priv;
  } t[TASKLET_CT];
};

RW_CF_TYPE_DEFINE("RW.uAgent RWTasklet Component Type", rwuagent_component_t);
RW_CF_TYPE_DEFINE("RW.uAgent RWTasklet Instance Type", rwuagent_instance_t);


class RwUagentEnsemble : public ::testing::Test {
protected:

  static void SetUpTestCase() {
    tenv.rwsched = rwsched_instance_new();
    ASSERT_TRUE(tenv.rwsched);

    // Register the RW.Init types
    RW_CF_TYPE_REGISTER(rwuagent_component_t);
    RW_CF_TYPE_REGISTER(rwuagent_instance_t);

    for (int i=0; i<TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      memset(ti, 0, sizeof(*ti));

      switch (i) {
      case TASKLET_ROUTER:
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/ROUTER/0");
        break;
      case TASKLET_BROKER:
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/BROKER/0");
        break;
      case TASKLET_UAGENT:          
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/UAGENT/0");
        break;          
      default:
        sprintf(ti->rwmsgpath, "/L/RWDTS_GTEST/MEMBER/%d", i-TASKLET_MEMBER_0);
        break;
      }

      ti->tasklet = rwsched_tasklet_new(tenv.rwsched);
      ASSERT_TRUE(ti->tasklet);
      
      ti->ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, ti->tasklet, rwtrace_init(), NULL);
      ASSERT_TRUE(ti->ep);

      const int usebro = TRUE;

      if (usebro) {
        rwmsg_endpoint_set_property_int(ti->ep, "/rwmsg/broker/enable", TRUE);

        // We'd like not to set this for router, but the broker's forwarding lookup doesn't support local tasklets
        rwmsg_endpoint_set_property_int(ti->ep, "/rwmsg/broker/shunt", TRUE);
      }

      ti->rwtasklet_info.rwsched_tasklet_info = ti->tasklet;
      switch (i) {
      case TASKLET_ROUTER:
        if (!ti->rwtasklet_info.rwlog_instance) {
          ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.UagentTest");
        }
        ti->dts = rwdts_test_router_init(ti->ep, rwsched_dispatch_get_main_queue(tenv.rwsched), &ti->rwtasklet_info, 0, ti->rwmsgpath);

        ASSERT_NE(ti->dts, (void*)NULL);

        break;
      case TASKLET_BROKER:
        if (!ti->rwtasklet_info.rwlog_instance) {
          ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.UagentTest");
        }
        rwmsg_broker_test_main(0, 1, 0, tenv.rwsched, ti->tasklet, NULL, TRUE/*mainq*/, &ti->bro);
        ASSERT_NE(ti->bro, (void*)NULL);        
        break;
      case TASKLET_UAGENT:{
        rwuagent_instance_t instance = (rwuagent_instance_t) RW_CF_TYPE_MALLOC0(sizeof(*instance), rwuagent_instance_t);
        

        struct rwtasklet_info_s t_info;
        memset (&t_info, 0, sizeof (t_info));
        t_info.rwsched_instance = tenv.rwsched;
        t_info.rwmsg_endpoint = ti->ep;
        t_info.rwsched_tasklet_info = ti->tasklet;
        
        instance->rwtasklet_info = &t_info;        
        ti->uagent = new Instance(instance);
        ti->uagent->setup_dom ("vehicle");
        ti->uagent->xml_mgr_->get_yang_model()->register_ypbc_schema(
          (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(Vehicle));

        ti->apih = rwdts_api_init_internal(NULL, NULL, ti->tasklet, tenv.rwsched, ti->ep, tenv.t[i].rwmsgpath, RWDTS_HARDCODED_ROUTER_ID, 0, NULL);
        // ATTN: ti->uagent->dts_h_ = ti->apih;
        ASSERT_NE(ti->apih, (void*)NULL);
        

        }
        
        break;
      default:
        ti->apih = rwdts_api_init_internal(NULL, NULL, ti->tasklet, tenv.rwsched, ti->ep, tenv.t[i].rwmsgpath, RWDTS_HARDCODED_ROUTER_ID, 0, NULL);
        ASSERT_NE(ti->apih, (void*)NULL);
        
      }
    }

    /* Run a short time to allow broker to accept connections etc */
    rwsched_dispatch_main_until(tenv.t[0].tasklet, 2/*s*/, NULL);
  }

  static void TearDownTestCase() {
    for (int i=TASKLET_CT-1; i>=0; i--) {
      struct tenv1::tenv_info *ti = &tenv.t[i];

      if (ti->dts) {
        rwdts_test_router_deinit(ti->dts);
        ti->dts = NULL;
      }

      if (ti->apih) {
        rw_status_t rs = rwdts_api_deinit(ti->apih);
        ASSERT_EQ(rs, RW_STATUS_SUCCESS);
        ti->apih = NULL;
      }

      if (ti->bro) {
        int r = rwmsg_broker_test_halt(ti->bro);
        ASSERT_TRUE(r);
      }

      if (ti->uagent) {
        delete ti->uagent;
      }

      ASSERT_TRUE(ti->ep);
      int r = rwmsg_endpoint_halt_flush(ti->ep, TRUE);
      ASSERT_TRUE(r);
      ti->ep = NULL;

      if (ti->rwtasklet_info.rwlog_instance) {
        rwlog_close(ti->rwtasklet_info.rwlog_instance,TRUE);
        ti->rwtasklet_info.rwlog_instance = NULL;
      }

      ASSERT_TRUE(ti->tasklet);
      rwsched_tasklet_free(ti->tasklet);
      ti->tasklet = NULL;
    }

    ASSERT_TRUE(tenv.rwsched);
    rwsched_instance_free(tenv.rwsched);
    tenv.rwsched = NULL;
  }

public:
  static struct tenv1 tenv;
};


struct tenv1 RwUagentEnsemble::tenv;
  
TEST_F(RwUagentEnsemble, DISABLED_CheckSetup) {
  ASSERT_TRUE(tenv.rwsched);

  /* This is the tasklet's DTS Router instance, only in router tasklet */
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER].dts);
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].dts);
  ASSERT_FALSE(tenv.t[TASKLET_UAGENT].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].dts);

  /* This is the tasklet's RWMsg Broker instance, only in broker tasklet */
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER].bro);
  ASSERT_TRUE(tenv.t[TASKLET_BROKER].bro);
  ASSERT_FALSE(tenv.t[TASKLET_UAGENT].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].bro);

  /* This is the tasklet's RWDts API Handle, in client, member, and router */
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].apih);
  ASSERT_TRUE(tenv.t[TASKLET_UAGENT].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_0].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_1].apih);

  /* The uagent instance should be true only for the UAGENT */
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].uagent);
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER].uagent);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].uagent);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].uagent);
  ASSERT_TRUE(tenv.t[TASKLET_UAGENT].uagent);
  
}


// TEST_F(RwUagentEnsemble, Foo) { }
// ... runs in a fixed environment with a client, router, broker, and some members, all tasklets etc
// ... dts apis are instantiated in those which needed it
// ... to setup: rwsched_dispatch_async_f to trigger a callback in a tasklet, typically on main queue (== CFRunLoop thread)
// ... to run: "rwsched_dispatch_main_until(tenv.t[0], 60/*s*/, &exitnow);"
// ... to end: set exitnow from app / test logic
// ... WARNING: there is no fixed ordering between tests

struct demo_state {
  uint32_t exitnow;
};
static void demo_member_setup_f(void *ctx) {
  struct demo_state *d = (struct demo_state *)ctx;
  TSTPRN("Starting ensemble demo test...\n");
  d=d;
}
static void demo_member_stop_f(void *ctx) {
  struct demo_state *d = (struct demo_state *)ctx;
  TSTPRN("Stopping ensemble demo test...\n");
  d->exitnow = 1;
}
TEST_F(RwUagentEnsemble, DISABLED_Demo) {
  struct demo_state d = { 0 };
  rwsched_dispatch_async_f(tenv.t[TASKLET_MEMBER_0].tasklet, rwsched_dispatch_get_main_queue(tenv.rwsched), &d, demo_member_setup_f);
  rwsched_dispatch_async_f(tenv.t[TASKLET_MEMBER_0].tasklet, rwsched_dispatch_get_main_queue(tenv.rwsched), &d, demo_member_stop_f);
  rwsched_dispatch_main_until(tenv.t[0].tasklet, 60, &d.exitnow);
  ASSERT_TRUE(d.exitnow);
}

enum treatment_e {
  TREATMENT_ACK=0,
  TREATMENT_NACK,
  TREATMENT_NA,
  TREATMENT_ERR, 
  TREATMENT_ASYNC
};

enum expect_result_e {
  XACT_RESULT_COMMIT=0,
  XACT_RESULT_ABORT,
  XACT_RESULT_ERROR
};



typedef struct client_operation_ {
  Client      *uagent_client;
  NetconfReq  *req;
} client_operation;


typedef void (*dom_validate_function)(rw_yang::XMLDocument::uptr_t& dom, ClientMsg *obj);

typedef struct uagent_test_state_ {
  uint32_t magic;
  uint32_t exitnow;
  uint8_t members_started;
  uint8_t members_participated;
  struct tenv1 *tenv;
  client_operation *cp;
  dom_validate_function validate;
} uagent_test_state;
  
uagent_test_state state;

class ClientMsgTest
    :public ClientMsg
{
 public:
  uagent_test_state *state_;
  
 public:
  ClientMsgTest(Instance* instance, uagent_test_state *state ):ClientMsg(instance),state_(state) {}
  
  void respond (DtsXaction  *xact,
                rwdts_xact_result_t* result,
                rw_yang::XMLDocument::uptr_t& resp,
                NetconfErrorList* nc_error)
  {

    if (state_->validate) {
      state_->validate(resp,this);
    }
    else {
      state_->exitnow = 1;
    }
  }
};

static void setup_uagent (void *ctxt)
{
  uagent_test_state *test = (uagent_test_state *)ctxt;
  ASSERT_TRUE (ctxt);
  dynamic_cast <rw_uagent::ClientMsg *> (test->cp->uagent_client)->netconf_request(test->cp->req, (NetconfRsp_Closure) 1, 0);
}

static void setup_member_get (void *ctxt)
{
  member_data *member = (member_data *) ctxt;
  ASSERT_TRUE (member);
  
  uagent_test_state *test = (uagent_test_state *) member->test;
  ASSERT_TRUE (test);

  rwdts_member_event_cb_t reg_cb;
  memset (&reg_cb, 0, sizeof (reg_cb));

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *) member->ti;
  ti->priv = *member;
  
  reg_cb.ud = &ti->priv;
  reg_cb.cb.prepare = member->cb.cb.prepare;

  rwdts_member_register (NULL, ti->apih, member->keyspec, &reg_cb,
                         member->desc, RWDTS_FLAG_PUBLISHER, NULL);

  
  test->members_started ++;

  if (test->members_started == TASKLET_MEMBER_COUNT) {
    // startup the clients query
    rwsched_dispatch_async_f(test->tenv->t[TASKLET_UAGENT].tasklet,
                             rwsched_dispatch_get_main_queue(test->tenv->rwsched),
                             test, setup_uagent);
  }
}

static void setup_member_config (void *ctxt)
{
  member_data *member = (member_data *) ctxt;
  ASSERT_TRUE (member);
  
  uagent_test_state *test = (uagent_test_state *) member->test;
  ASSERT_TRUE (test);

  rwdts_member_event_cb_t reg_cb;
  memset (&reg_cb, 0, sizeof (reg_cb));
  
  reg_cb.ud = ctxt;
  reg_cb.cb.prepare = member->cb.cb.prepare;

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *) member->ti;
  ti->priv = *member;

  rwdts_member_register (NULL, ti->apih, member->keyspec, &reg_cb, member->desc, RWDTS_FLAG_SUBSCRIBER, NULL);
  test->members_started ++;

  if (test->members_started == TASKLET_MEMBER_COUNT) {
    // startup the clients query
    rwsched_dispatch_async_f(test->tenv->t[TASKLET_UAGENT].tasklet,
                             rwsched_dispatch_get_main_queue(test->tenv->rwsched),
                             test, setup_uagent);
  }
}

static rwdts_member_rsp_code_t memberapi_test_prepare(const rwdts_xact_info_t* xact_info,
                                                      RWDtsQueryAction         action,
                                                      const rw_keyspec_path_t*      key,
                                                      const ProtobufCMessage*  msg, 
                                                      uint32_t credits, 
                                                      void *getnext_ptr) 
{

  return RWDTS_ACTION_OK;
}

// A member data..
member_data case1 = {
  .keyspec = (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(Vehicle_data_Car),
  .desc = RWPB_G_MSG_PBCMD(Vehicle_data_Car),
  .pb = 0,
  .cb = 
  {
    .cb = {
      .prepare = memberapi_test_prepare
    }
  }
};

RWPB_T_MSG(Vehicle_data_Car) car;
RWPB_T_MSG(Vehicle_data_Car_Models) model;
RWPB_T_MSG(Vehicle_data_Car_Models) *models[1];
// Start the test..
RWPB_T_MSG(Vehicle_data_Car_Models) corolla = {
  { PROTOBUF_C_MESSAGE_INIT(RWPB_G_MSG_PBCMD(Vehicle_data_Car_Models), PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0)},
  .name = strdup ("Corolla"),
  .has_capacity = 1,
  .capacity = 4
};

RWPB_T_MSG(Vehicle_data_Car_Models) camry = {
  { PROTOBUF_C_MESSAGE_INIT(RWPB_G_MSG_PBCMD(Vehicle_data_Car_Models), PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0)},
  .name = strdup ("Camry"),
  .has_capacity = 1,
  .capacity = 5
};

RWPB_T_MSG(Vehicle_data_Car_Models) sienna = {
  { PROTOBUF_C_MESSAGE_INIT(RWPB_G_MSG_PBCMD(Vehicle_data_Car_Models), PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0)},
  .name = strdup ("Sienna"),
  .has_capacity = 1,
  .capacity = 7
};

RWPB_T_MSG(Vehicle_data_Car_Models) civic = {
  { PROTOBUF_C_MESSAGE_INIT(RWPB_G_MSG_PBCMD(Vehicle_data_Car_Models), PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0)},
  .name = strdup ("Civic"),
  .has_capacity = 1,
  .capacity = 4
};

RWPB_T_MSG(Vehicle_data_Car_Models) accord = {
  { PROTOBUF_C_MESSAGE_INIT(RWPB_G_MSG_PBCMD(Vehicle_data_Car_Models), PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0)},
  .name = strdup ("Accord"),
  .has_capacity = 1,
  .capacity = 5
};

RWPB_T_MSG(Vehicle_data_Car_Models) pilot = {
  { PROTOBUF_C_MESSAGE_INIT(RWPB_G_MSG_PBCMD(Vehicle_data_Car_Models), PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0)},
  .name = strdup("Pilot"),
  .has_capacity = 1,
  .capacity = 8
};

RWPB_T_MSG(Vehicle_data_Car_Models) *toyota_s[] = {&corolla, &sienna, &camry};
RWPB_T_MSG(Vehicle_data_Car_Models) *honda_s[] = {&accord, &civic, &pilot};

// Protobuf for a toyota car?
RWPB_T_MSG(Vehicle_data_Car) toyota = {
  { PROTOBUF_C_MESSAGE_INIT(RWPB_G_MSG_PBCMD(Vehicle_data_Car), PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0)},
  .brand = strdup ("Toyota"),
  .n_models = 3,
  .models = toyota_s
};

RWPB_T_MSG(Vehicle_data_Car) honda = {
  { PROTOBUF_C_MESSAGE_INIT(RWPB_G_MSG_PBCMD(Vehicle_data_Car), PROTOBUF_C_FLAG_REF_STATE_GLOBAL, 0)},
  .brand = strdup("Honda"),
  .n_models = 3,
  .models = honda_s
};

XmlBlob blob;

std::string xml_string;
NetconfReq n_req;

client_operation client;

TEST_F (RwUagentEnsemble, DISABLED_MsgClientSimpleConfig)
{
  // Build an XML string.

 
  RWPB_F_MSG_INIT(Vehicle_data_Car,&car);

  car.brand = (char *) "Toyota";
  car.n_models = 1;


  
  RWPB_F_MSG_INIT(Vehicle_data_Car_Models,&model);
  

  models[0] = &model;

  model.name = (char *) "Corolla";
  car.models = models;


  rw_uagent::Instance *inst = tenv.t[TASKLET_UAGENT].uagent;
  ASSERT_TRUE(inst);
  xml_string.clear();
  rw_status_t rs = inst->xml_mgr_->pb_to_xml((ProtobufCMessage*) &car, xml_string);
  ASSERT_EQ (rs, RW_STATUS_SUCCESS);

  ClientMsgTest *u_client = new ClientMsgTest(inst, &state);
  ASSERT_TRUE (u_client);

  
  client.uagent_client = u_client;
  client.req = &n_req;

  memset (&state, 0, sizeof (state));


  blob.xml_blob = (char *) xml_string.c_str();
  

  n_req.has_operation = 1;
  n_req.operation = nc_edit_config;
  n_req.xml_blob = &blob;

  state.cp = &client;
  state.tenv = &tenv;
  
  // Startup members
  for (int i = 0; i < TASKLET_MEMBER_COUNT; i++) {    
    case1.ti = &tenv.t[TASKLET_MEMBER_0+i];
    case1.test = &state;
    rwsched_dispatch_async_f (tenv.t[TASKLET_MEMBER_0+i].tasklet,
                              rwsched_dispatch_get_main_queue(tenv.rwsched),
                              &case1,setup_member_config);  
  }

  // Wait for the members to be up
  rwsched_dispatch_main_until(tenv.t[0].tasklet, 60, &state.exitnow);
  
  ASSERT_TRUE (state.exitnow);

}
  


static rwdts_member_rsp_code_t memberapi_test_get(const rwdts_xact_info_t* xact_info,
                                                  RWDtsQueryAction         action,
                                                  const rw_keyspec_path_t*      key,
                                                  const ProtobufCMessage*  msg,
                                                  uint32_t credits,
                                                  void *getnext_ptr) 
{
  RW_ASSERT(xact_info);
  member_data *member = (member_data *) xact_info->ud;

  if (member->pb) {
    const ProtobufCMessage *msgs[1] = {member->pb};
    rwdts_member_query_rsp_t rsp;
    RW_ZERO_VARIABLE(&rsp);

    rsp.ks = member->resp_keyspec;
    rsp.n_msgs = 1;
    rsp.msgs = (ProtobufCMessage**) msgs;

    rsp.evtrsp = RWDTS_EVTRSP_ACK;
    rw_status_t rs = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  return RWDTS_ACTION_OK;
}

// A member data..
member_data case2 = {
  .keyspec = (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(Vehicle_data_Car),
  .desc = RWPB_G_MSG_PBCMD(Vehicle_data_Car),
  .pb = 0,
  .cb = 
  {
    .cb = {
      .prepare = memberapi_test_get
    }
  }
};

void validate_get(rw_yang::XMLDocument::uptr_t& rsp, ClientMsg *foo)
{
  ASSERT_TRUE (rsp.get());
  XMLNode *node = rsp->get_root_node();
  UNUSED(node);
  ClientMsgTest *client = static_cast<ClientMsgTest*>(foo);
  client->state_->exitnow = 1;
}

TEST_F (RwUagentEnsemble, DISABLED_MsgClientSimpleGet)
{
  RWPB_M_PATHSPEC_DECL_INIT(Vehicle_data_Car,toyota_ks);
  // Keyspec for honda..
  RWPB_M_PATHSPEC_DECL_INIT(Vehicle_data_Car,honda_ks);
  toyota_ks.dompath.path000.has_key00 = 1;
  toyota_ks.dompath.path000.key00.brand = toyota.brand;

  honda_ks.dompath.path000.has_key00 = 1;
  honda_ks.dompath.path000.key00.brand = honda.brand;

  // The request - all cars?
   
  RWPB_F_MSG_INIT(Vehicle_data_Car,&car);

  rw_uagent::Instance *inst = tenv.t[TASKLET_UAGENT].uagent;
  ASSERT_TRUE(inst);
  
  xml_string.clear();
  rw_status_t rs = inst->xml_mgr_->pb_to_xml((ProtobufCMessage*) &car, xml_string);
  ASSERT_EQ (rs, RW_STATUS_SUCCESS);

  ClientMsgTest *u_client = new ClientMsgTest(inst, &state);
  ASSERT_TRUE (u_client);

  client.uagent_client = u_client;
  client.req = &n_req;

  memset (&state, 0, sizeof (state));


  blob.xml_blob = (char *) xml_string.c_str();
  

  n_req.has_operation = 1;
  n_req.operation = nc_get;
  n_req.xml_blob = &blob;

  state.cp = &client;
  state.tenv = &tenv;
  state.validate = validate_get;
  
  // Startup members
  for (int i = 0; i < TASKLET_MEMBER_COUNT; i++) {    
    case2.ti = &tenv.t[TASKLET_MEMBER_0+i];
    case2.test = &state;

    if (i == 0) {
      case2.pb = (ProtobufCMessage *) &toyota;
      case2.resp_keyspec = (rw_keyspec_path_t *)&toyota_ks;

    } else if ( i == 1) {
      case2.resp_keyspec = (rw_keyspec_path_t *)&honda_ks;
      case2.pb = (ProtobufCMessage *) &honda;
    }
    
    rwsched_dispatch_async_f (tenv.t[TASKLET_MEMBER_0+i].tasklet,
                              rwsched_dispatch_get_main_queue(tenv.rwsched),
                              &case2,setup_member_get);  
  }

  // Wait for the members to be up
  rwsched_dispatch_main_until(tenv.t[0].tasklet, 60, &state.exitnow);
  
  ASSERT_TRUE (state.exitnow);

}

static void assert_test()
{
  ASSERT_TRUE (0);
}
// A callback that should never be called if test is success
static rwdts_member_rsp_code_t memberapi_test_never_called(const rwdts_xact_info_t* xact_info,
                                                           RWDtsQueryAction         action,
                                                           const rw_keyspec_path_t*      key,
                                                           const ProtobufCMessage*  msg,
                                                           uint32_t credits,
                                                           void *getnext_ptr) 
{
  assert_test();
  return RWDTS_ACTION_OK;
}

// A callback used when a member subscribers and publishes a path, but expects only
// ADVISE queries
static rwdts_member_rsp_code_t publisher_and_subscriber_callback(const rwdts_xact_info_t* xact_info,
                                                                 RWDtsQueryAction         action,
                                                                 const rw_keyspec_path_t*      key,
                                                                 const ProtobufCMessage*  msg,
                                                                 uint32_t credits,
                                                                 void *getnext_ptr) 
{
  RW_ASSERT(xact_info);
  EXPECT_TRUE (action != RWDTS_QUERY_READ);
  member_data *member = (member_data *) xact_info->ud;
  EXPECT_TRUE (member);
  
  uagent_test_state *test = (uagent_test_state *) member->test;
  EXPECT_TRUE (test);

  test->members_participated++;

  return RWDTS_ACTION_OK;
}

static void setup_member (void *ctxt)
{
  member_data *member = (member_data *) ctxt;
  //ASSERT_TRUE (member);
  
  uagent_test_state *test = (uagent_test_state *) member->test;
  //ASSERT_TRUE (test);

  rwdts_member_event_cb_t reg_cb;
  memset (&reg_cb, 0, sizeof (reg_cb));
  
  reg_cb.ud = ctxt;
  reg_cb.cb.prepare = member->cb.cb.prepare;

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *) member->ti;
  ti->priv = *member;

  rwdts_member_register (NULL, ti->apih, member->keyspec, &reg_cb, member->desc, member->flags, NULL);
  test->members_started++;

  if (test->members_started == 2) {
    rwsched_dispatch_async_f(test->tenv->t[TASKLET_UAGENT].tasklet,
                             rwsched_dispatch_get_main_queue(test->tenv->rwsched),
                             test, setup_uagent);
  }
}

static void uagent_test_get_event_cb (rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  member_data *member = (member_data *) ud;
  ASSERT_TRUE (member);
  
  uagent_test_state *test = (uagent_test_state *) member->test;
  ASSERT_TRUE (test);

  ASSERT_EQ (test->members_participated, 1);
  // Add validation of the results obtained
  test->exitnow = 1;
}

// Setup a member that will get data at a keyspec
static void setup_getter (void *ctxt)
{
  member_data *member = (member_data *) ctxt;
  ASSERT_TRUE (member);
  
  uagent_test_state *test = (uagent_test_state *) member->test;
  ASSERT_TRUE (test);


  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *) member->ti;
  ti->priv = *member;

  rwdts_xact_t *xact = rwdts_api_query_ks(ti->apih,                      /* api_handle */
                                           member->keyspec,               /* key */
                                           RWDTS_QUERY_READ,              /* action */
                                           0,                             // flags
                                           uagent_test_get_event_cb,      // callback 
                                           &ti->priv,                     // ud 
                                           0);                            // msg
  ASSERT_TRUE (xact!=NULL);
}

  
member_data md[5];
void validate_registration(rw_yang::XMLDocument::uptr_t& rsp, ClientMsg *foo)
{
  // Config done = start the thing that does a get
  ClientMsgTest *client = static_cast<ClientMsgTest*>(foo);
  memset (&md[2], 0, sizeof (md[0]));
  rw_keyspec_path_t *ks;
  rw_keyspec_path_create_dup ((rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(Vehicle_data_Car_Models), NULL ,
                         &ks);
  rw_keyspec_path_set_category (ks, NULL , RW_SCHEMA_CATEGORY_CONFIG);
  
  md[2].keyspec = ks;
  md[2].test = &state;
  md[2].ti = &client->state_->tenv->t[TASKLET_MEMBER_2];
  
  rwsched_dispatch_async_f (client->state_->tenv->t[TASKLET_MEMBER_2].tasklet,
                            rwsched_dispatch_get_main_queue(client->state_->tenv->rwsched),
                            &md[2],setup_getter);
}

TEST_F (RwUagentEnsemble, DISABLED_Registration)
{
  RWPB_M_PATHSPEC_DECL_INIT(Vehicle_data_Car,toyota_ks);

  // Keyspec for honda..
  RWPB_M_PATHSPEC_DECL_INIT(Vehicle_data_Car,honda_ks);
  toyota_ks.dompath.path000.has_key00 = 1;
  toyota_ks.dompath.path000.key00.brand = toyota.brand;

  honda_ks.dompath.path000.has_key00 = 1;
  honda_ks.dompath.path000.key00.brand = honda.brand;

  // The request - all cars?

  // STEP 1 - Configure: All clients in configure mode

  rw_uagent::Instance *inst = tenv.t[TASKLET_UAGENT].uagent;
  ASSERT_TRUE(inst);
  
  xml_string.clear();
  rw_status_t rs = inst->xml_mgr_->pb_to_xml((ProtobufCMessage*) &honda, xml_string);
  ASSERT_EQ (rs, RW_STATUS_SUCCESS);
  
  ClientMsgTest *u_client = new ClientMsgTest(inst, &state);
  ASSERT_TRUE (u_client);

  
  client.uagent_client = u_client;
  client.req = &n_req;

  memset (&state, 0, sizeof (state));

  blob.xml_blob = (char *) xml_string.c_str();
  
  n_req.has_operation = 1;
  n_req.operation = nc_edit_config;
  n_req.xml_blob = &blob;

  state.cp = &client;
  state.tenv = &tenv;
  state.validate = validate_registration;
  // Startup 2 members - both subscribe to the configuration, Client 2 publishes
  // a different path.

  // The member data is copied into a global variable - so can be local
  memset (md, 0, sizeof (md));
  rw_keyspec_path_t *ks = 0;
  
  rs = rw_keyspec_path_create_dup ((rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(Vehicle_data_Car), NULL ,&ks);

  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  rw_keyspec_path_set_category (ks, NULL , RW_SCHEMA_CATEGORY_CONFIG);
  md[0].keyspec = ks;
  md[0].desc = RWPB_G_MSG_PBCMD(Vehicle_data_Car);

  md[0].cb.cb.prepare = publisher_and_subscriber_callback;
  

  // member 0 - just a subscriber to car
  md[0].flags = RWDTS_FLAG_SUBSCRIBER;
  md[0].ti = &tenv.t[TASKLET_MEMBER_0];
  md[0].test = &state;
  rwsched_dispatch_async_f (tenv.t[TASKLET_MEMBER_0].tasklet,
                            rwsched_dispatch_get_main_queue(tenv.rwsched),
                            &md[0],setup_member);  

  rw_keyspec_path_t *ks1 = 0;
  rs = rw_keyspec_path_create_dup ((rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(Vehicle_data_Car), NULL ,&ks1);

  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  rw_keyspec_path_set_category (ks1, NULL , RW_SCHEMA_CATEGORY_DATA);

  md[1] = md[0];
  md[1].cb.cb.prepare = memberapi_test_never_called;
  md[1].keyspec = ks1;
  md[1].flags = RWDTS_FLAG_SUBSCRIBER | RWDTS_FLAG_PUBLISHER;
  md[1].ti = &tenv.t[TASKLET_MEMBER_1];
  rwsched_dispatch_async_f (tenv.t[TASKLET_MEMBER_0].tasklet,
                            rwsched_dispatch_get_main_queue(tenv.rwsched),
                            &md[1],setup_member);  

  // Wait for the members to be up
  rwsched_dispatch_main_until(tenv.t[0].tasklet, 60, &state.exitnow);

  ASSERT_TRUE (state.exitnow);

}


#endif
RWUT_INIT();
