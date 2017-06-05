#include <rwlib.h>

#define TSTPRN(args...) {                       \
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

#define TEST_MAX_RECOVER_AUDIT 8
#define REDIS_CLUSTER_PORT_INCR 10000
struct venv{
  int checked;
  int val;
};
extern struct venv venv_g;

/* A slight abuse of "unit" testing, to permit DTS testing in a
   simplified multi-tasklet environment. */
typedef enum {
  TASKLET_ROUTER=0,
  TASKLET_ROUTER_3,
  TASKLET_ROUTER_4,
  TASKLET_BROKER,
  TASKLET_NONMEMBER,
  TASKLET_CLIENT,

#define TASKLET_IS_MEMBER_OLD(t) (((t) >= TASKLET_MEMBER_0) && ((t) <= TASKLET_MEMBER_2))
#define TASKLET_IS_MEMBER(t) (((t) >= TASKLET_MEMBER_0))

  TASKLET_MEMBER_0,
  TASKLET_MEMBER_1,
  TASKLET_MEMBER_2,
#define TASKLET_MEMBER_CT (3)
  TASKLET_MEMBER_3,
  TASKLET_MEMBER_4,
#define TASKLET_MEMBER_CT_NEW (5)

  TASKLET_CT
} twhich_t;

typedef struct cb_track_s {
  int reg_ready;
  int reg_ready_old;
  int prepare;
  int precommit;
  int commit;
  int abort;
  int xact_init;
  int xact_deinit;
  int validate;
  int apply;
} cb_track_t;

struct tenv1 {
  rwsched_instance_ptr_t rwsched;
  rwvx_instance_t *rwvx;
  int testno;

  struct tenv_info {
    twhich_t tasklet_idx;
    int member_idx;
    char rwmsgpath[1024];
    rwsched_tasklet_ptr_t tasklet;
    //    rwsched_dispatch_queue_t rwq;
    rwmsg_endpoint_t *ep;
    rwtasklet_info_t rwtasklet_info;
    rwdts_xact_info_t *saved_xact_info;

    // each tasklet might have one or more of:
    rwdts_router_t *dts;
    rwdts_api_t *apih;
    rwmsg_broker_t *bro;
    uint32_t  member_responses;
    struct {
      const rwdts_xact_info_t* xact_info;
      RWDtsQueryAction action;
      rw_keyspec_path_t *key;
      ProtobufCMessage *msg;
      rwdts_xact_block_t *block;

      int rspct;
    } async_rsp_state;

    void *ctx;                        // test-specific state ie queryapi_state

    uint32_t api_state_change_called;
    union {
      int prepare_cb_cnt;
      uint32_t miscval;
    };
    cb_track_t cb;

  } t[TASKLET_CT];
};


enum treatment_e {
  TREATMENT_ACK=100,
  TREATMENT_NACK,
  TREATMENT_NA,
  TREATMENT_ERR,
  TREATMENT_ASYNC,
  TREATMENT_ASYNC_NACK,
  TREATMENT_BNC,
  TREATMENT_PRECOMMIT_NACK,
  TREATMENT_PRECOMMIT_NULL,
  TREATMENT_COMMIT_NACK,
  TREATMENT_ABORT_NACK,
  TREATMENT_PREPARE_DELAY
};

enum expect_result_e {
  XACT_RESULT_COMMIT=0,
  XACT_RESULT_ABORT,
  XACT_RESULT_ERROR
};

enum reroot_test_e {
  REROOT_FIRST=0,
  REROOT_SECOND,
  REROOT_THIRD
};


class RWDtsEnsemble : public ::testing::Test {
 protected:
  
  static void SetUpTestCase();
  static void TearDownTestCase();
  
  static int rwdts_test_get_running_xact_count();
  
  
  void SetUp();
  
  void TearDown();
  
  
 public:
  static struct tenv1 tenv;
};



rw_status_t
rwdts_member_deregister_all(rwdts_api_t* apih);
