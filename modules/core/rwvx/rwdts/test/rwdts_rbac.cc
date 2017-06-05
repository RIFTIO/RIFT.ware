/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwdts_rbac.cc
 * @author RIFT.io <info@riftio.com>
 * @date 11/18/2013
 * @brief Google test cases for testing rwdts
 *
 * @details Google test cases for testing rwmsg.
 */

/**
 * Step 1: Include the necessary header files
 */
#include <sys/socket.h>
#include <sys/un.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <limits.h>
#include <cstdlib>
#include <sys/resource.h>
#include <ctime>
#include<iostream>

#include <valgrind/valgrind.h>

#include "rwut.h"
#include "rwlib.h"
#include "rw_dl.h"
#include "rw_sklist.h"

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
#include <rwtasklet.h>

#include <rwmsg.h>
#include <rwmsg_int.h>
#include <rwmsg_broker.h>

#include <rwdts.h>
#include <rwdts_int.h>
#include <rwdts_router.h>
#include <dts-test.pb-c.h>
#include <testdts-rw-fpath.pb-c.h>
#include <testdts-rw-stats.pb-c.h>
#include <rwdts_redis.h>
#include <sys/prctl.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include <rwdts_appconf_api.h>
#include <rwdts_xpath.h>
#include "rwdts_ensemble.hpp"

struct tenv1 RWDtsEnsemble::tenv;
#define DTS_TEST_RBAC_MAX_REG_PER_MEMBER 20
#define DTS_TEST_RBAC_MAX_APPCONF_PER_MEMBER 20
#define DTS_TEST_RBAC_MAX_PREPARE 20
#define RWDTS_QUERY_MAX RWDTS_QUERY_RPC+1
#define RWDTS_APPCONF_ACTION_MAX RWDTS_APPCONF_ACTION_RECOVER

struct rbac_test_member_data;

enum prepare_action{
  UPDATE_ON_PREPARE = 1,
  UPDATE_ON_PREPARE_WITHOUT_ADVISE = 2,
};

struct rbac_scratchpad{
  int              num_prepare;
  ProtobufCMessage *prepare_msg[DTS_TEST_RBAC_MAX_PREPARE];
  rw_keyspec_path_t *prepare_keyspec[DTS_TEST_RBAC_MAX_PREPARE];
  int                prepare_action[DTS_TEST_RBAC_MAX_PREPARE];
};

struct rbac_test_member_reg_stat{
  int num_regready;
  int num_prepare[RWDTS_QUERY_MAX];
  int num_precommit;
  int num_commit;
  int num_abort;
};


struct rbac_test_member_appconf_stat{
  int num_init;
  int num_deinit;
  int num_validate;
  int num_apply[RWDTS_APPCONF_ACTION_MAX];
};


struct rbac_test_member_reg_data{
  rwdts_member_reg_handle_t        regh;
  rw_keyspec_path_t                *keyspec;
  struct rbac_test_member_data     *member;
  struct rbac_test_member_reg_stat happened;
  struct rbac_test_member_reg_stat expected;
  struct rbac_test_member_reg_stat retval;
  bool                             publish;
};


struct rbac_test_member_appconf{
  rwdts_appconf_t                      *ac;
  struct rbac_test_member_data         *member;
  struct rbac_test_member_appconf_stat expected;
  struct rbac_test_member_appconf_stat happened;
  struct rbac_test_member_appconf_stat retval;
  int                              num_reg;
  struct rbac_test_member_reg_data reg[DTS_TEST_RBAC_MAX_REG_PER_MEMBER];
};


struct rbac_test_member_data {
  bool                             used;
  rwsched_dispatch_queue_t         rwq;
  int                              num_reg;
  struct rbac_test_member_reg_data reg[DTS_TEST_RBAC_MAX_REG_PER_MEMBER];
  int                              num_appconf;
  struct rbac_test_member_appconf  appconf[DTS_TEST_RBAC_MAX_APPCONF_PER_MEMBER];
};



struct rbac_test_data {
#define QAPI_STATE_MAGIC (0x0a0e0402)
  uint32_t magic;
  int      testno;
  char     test_name[128];
  struct tenv1 *tenv;

  uint32_t      exitnow;
  char test_case[128];
  
  struct rbac_test_member_data member[TASKLET_CT];
};

static void init_test_state(struct rbac_test_data *s)
{
  int i;
  s->magic = QAPI_STATE_MAGIC;
  s->tenv = &RWDtsEnsemble::tenv;
  s->testno = s->tenv->testno;
  //s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  strncpy(s->test_name,  test_info->name(), sizeof(s->test_name) - 1);
  strncpy(s->test_case,  test_info->test_case_name(), sizeof(s->test_case) - 1);
  
  for (i = 0; i < TASKLET_CT; i++){
    memset(&s->member[i], 0, sizeof(s->member[i]));
  }
}

static bool rbac_test_completed_check(struct rbac_test_data *s)
{
  int i;
  int reg_iter;
  int appconf_iter;
  int action;

  
  for (i = 0; i < TASKLET_CT; i++){
    if (s->member[i].used){
      for (reg_iter = 0; reg_iter < s->member[i].num_reg; reg_iter++){
        for (action = 1; action < RWDTS_QUERY_MAX; action++){
          if (s->member[i].reg[reg_iter].expected.num_prepare[action]){
            if (s->member[i].reg[reg_iter].expected.num_prepare[action] != s->member[i].reg[reg_iter].happened.num_prepare[action]){
              return false;
            }
          }
        }
        if (s->member[i].reg[reg_iter].expected.num_precommit){
          if (s->member[i].reg[reg_iter].expected.num_precommit != s->member[i].reg[reg_iter].happened.num_precommit){
            return false;
          }
        }
        if (s->member[i].reg[reg_iter].expected.num_commit){
          if (s->member[i].reg[reg_iter].expected.num_commit != s->member[i].reg[reg_iter].happened.num_commit){
            return false;
          }
        }
        if (s->member[i].reg[reg_iter].expected.num_abort){
          if (s->member[i].reg[reg_iter].expected.num_abort != s->member[i].reg[reg_iter].happened.num_abort){
            return false;
          }
        }
        if (s->member[i].reg[reg_iter].expected.num_regready){
          if (s->member[i].reg[reg_iter].expected.num_regready != s->member[i].reg[reg_iter].happened.num_regready){
            return false;
          }
        }
      }
      for (appconf_iter = 0; appconf_iter < s->member[i].num_appconf; appconf_iter++){
        if (s->member[i].appconf[appconf_iter].expected.num_init){
          if (s->member[i].appconf[appconf_iter].expected.num_init != s->member[i].appconf[appconf_iter].happened.num_init){
            return false;
          }
        }
        if (s->member[i].appconf[appconf_iter].expected.num_deinit){
          if (s->member[i].appconf[appconf_iter].expected.num_deinit != s->member[i].appconf[appconf_iter].happened.num_deinit){
            return false;
          }
        }
        if (s->member[i].appconf[appconf_iter].expected.num_validate){
          if (s->member[i].appconf[appconf_iter].expected.num_validate != s->member[i].appconf[appconf_iter].happened.num_validate){
            return false;
          }
        }
        //AKKI ideally this should be an array for different actions i.e install/reconcile
        for (action = 0; action < RWDTS_APPCONF_ACTION_MAX; action++){
          if (s->member[i].appconf[appconf_iter].expected.num_apply[action]){
            if (s->member[i].appconf[appconf_iter].expected.num_apply[action] != s->member[i].appconf[appconf_iter].happened.num_apply[action]){
              return false;
            }
          }
        }
      }
    }
  }

  s->exitnow = 1;
  
  return true;
}






static void init_test_member_state(struct rbac_test_data *s, twhich_t which)
{
  struct tenv1::tenv_info *ti = NULL;

  s->tenv->t[which].ctx = (void *)s;
  s->member[which].used = 1;
  s->member[which].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  ti = &s->tenv->t[which];
  if (ti->apih){
    rwdts_member_deregister_all(ti->apih);
  }
}


static struct rbac_test_member_reg_data*
rbac_find_reg_data(struct tenv1::tenv_info *ti,
                   rwdts_member_reg_handle_t reg)
{
  int i;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  struct rbac_test_member_data *member = &s->member[ti->tasklet_idx];

  for (i = 0; i < member->num_reg; i++){
    if (member->reg[i].regh == reg){
      break;
    }
  }
  RW_ASSERT(i < member->num_reg);
  
  return &member->reg[i]; 
}

static struct rbac_test_member_reg_data*
rbac_find_reg_data_in_ac(struct tenv1::tenv_info *ti,
                         struct rbac_test_member_appconf *appconf,
                         rwdts_member_reg_handle_t reg)
{
  int i;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  for (i = 0; i < appconf->num_reg; i++){
    if (appconf->reg[i].regh == reg){
      break;
    }
  }
  RW_ASSERT(i < appconf->num_reg);
  
  return &appconf->reg[i]; 
}

static struct rbac_test_member_appconf*
rbac_find_appconf_member(struct tenv1::tenv_info *ti,
                         rwdts_appconf_t* ac)
{
  int i;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  struct rbac_test_member_data *member = &s->member[ti->tasklet_idx];

  for (i = 0; i < member->num_appconf; i++){
    if (member->appconf[i].ac == ac){
      break;
    }
  }
  RW_ASSERT(i < member->num_appconf);
  
  return &member->appconf[i];
}

static void*
rbac_xact_init(rwdts_appconf_t* ac,
               rwdts_xact_t*    xact,
               void*            ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct rbac_test_member_appconf *appconf = rbac_find_appconf_member(ti, ac);
  RW_ASSERT(appconf != NULL);
  struct rbac_scratchpad *scratch;

  scratch =(struct rbac_scratchpad *) RW_MALLOC0(sizeof(*scratch));
  
  return scratch;
}



static void 
rbac_xact_deinit(rwdts_appconf_t* ac,
                 rwdts_xact_t*    xact,
                 void*            ud,
                 void*            scratch_in)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct rbac_test_member_appconf *appconf = rbac_find_appconf_member(ti, ac);
  RW_ASSERT(appconf != NULL);
  struct rbac_scratchpad *scratch = ( struct rbac_scratchpad *)scratch_in;
  int i;

  for (i = 0; i < scratch->num_prepare; i++){
    if (scratch->prepare_msg[i]){
      protobuf_c_message_free_unpacked(NULL, scratch->prepare_msg[i]);
      scratch->prepare_msg[i] = NULL;
    }
    if (scratch->prepare_keyspec[i]){
      rw_keyspec_path_free(scratch->prepare_keyspec[i], NULL);
      scratch->prepare_keyspec[i] = NULL;
    }
    
  }
  return;
}

static void
rbac_validate(rwdts_api_t*     apih,
              rwdts_appconf_t* ac,
              rwdts_xact_t*    xact,
              void*            ctx,
              void*            scratch_in)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct rbac_test_member_appconf *appconf = rbac_find_appconf_member(ti, ac);
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;

  RW_ASSERT(appconf != NULL);
  appconf->happened.num_validate++;
  
  rbac_test_completed_check(s);
}

static void
rbac_apply(rwdts_api_t*           apih,
           rwdts_appconf_t*       ac,
           rwdts_xact_t*          xact,
           rwdts_appconf_action_t action,
           void*                  ctx,
           void*                  scratch_in)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct rbac_test_member_appconf *appconf = rbac_find_appconf_member(ti, ac);
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  RW_ASSERT(appconf != NULL);

  appconf->happened.num_apply[action]++;
  rbac_test_completed_check(s);
  
}


static void
member_config_prepare(rwdts_api_t *apih,
                      rwdts_appconf_t *ac,
                      rwdts_xact_t *xact,
                      const rwdts_xact_info_t *xact_info,
                      rw_keyspec_path_t *keyspec,
                      const ProtobufCMessage *pbmsg,
                      void *ctx,
                      void *scratch_in)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct rbac_test_member_appconf *appconf = rbac_find_appconf_member(ti, ac);
  struct rbac_test_member_reg_data *regdata = rbac_find_reg_data_in_ac(ti, appconf,
                                                                       xact_info->regh);
  struct rbac_scratchpad *scratch = (struct rbac_scratchpad *)scratch_in;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  
  RW_ASSERT(appconf != NULL);
  //IT DOES NOT TELL THE REGISTRATION FOR WHICH THE PREPARE IS GOT....
  if (pbmsg)
    scratch->prepare_msg[scratch->num_prepare] =
        protobuf_c_message_duplicate_allow_deltas(NULL,
                                                  (const ProtobufCMessage *)pbmsg,
                                                  pbmsg->descriptor);
  scratch->prepare_keyspec[scratch->num_prepare] = NULL;
  rw_keyspec_path_create_dup((const rw_keyspec_path_t*)keyspec, NULL,
                             &scratch->prepare_keyspec[scratch->num_prepare]);
  //scratch->prepare_action[scratch->num_prepare] = ;
  
  scratch->num_prepare++;
  
  RW_ASSERT(regdata != NULL);
  //regdata->happened.num_prepare[]++;
  //create another blk and append to the same xact
  rwdts_xact_block_t *blk = NULL;
  uint32_t flags = RWDTS_XACT_FLAG_END | RWDTS_XACT_FLAG_ADVISE | RWDTS_XACT_FLAG_TRACE;
  rw_status_t status;
  rw_keyspec_path_t *ks = NULL;
  ProtobufCMessage *msg = NULL;
  
  blk = rwdts_xact_block_create(xact_info->xact);
  ASSERT_TRUE(blk);
  if (keyspec->base.descriptor == RWPB_G_PATHSPEC_PBCMD(DtsTest_DtsTestRbacRoleConfig)){
    RWPB_T_MSG(DtsTest_DtsTestRbacRole) *role;
    RWPB_T_PATHSPEC(DtsTest_DtsTestRbacRole) *rolepath = NULL;
    role = (RWPB_T_MSG(DtsTest_DtsTestRbacRole) *)RW_MALLOC0(sizeof(*role));
    RWPB_F_MSG_INIT(DtsTest_DtsTestRbacRole, role);
    
    msg = (ProtobufCMessage *)&role->base;
    ks = NULL;
    rw_keyspec_path_create_dup(((const rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacRole)), NULL, &ks);
    rw_keyspec_path_set_category(ks, NULL, RW_SCHEMA_CATEGORY_DATA);
    rolepath = (RWPB_T_PATHSPEC(DtsTest_DtsTestRbacRole) *)ks;
    rolepath->has_dompath = 1;
    rolepath->dompath.path002.has_key00 = 1;
    rolepath->dompath.path002.key00.role = strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacRoleConfig)*)keyspec)->dompath.path002.key00.role);
    role->role = strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacRoleConfig)*)keyspec)->dompath.path002.key00.role);
  }else if (keyspec->base.descriptor == RWPB_G_PATHSPEC_PBCMD(DtsTest_DtsTestRbacUserConfig)){
    RWPB_T_MSG(DtsTest_DtsTestRbacUser) *user;
    
    RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUser) *userpath = NULL;
    user = (RWPB_T_MSG(DtsTest_DtsTestRbacUser) *)RW_MALLOC0(sizeof(*user));
    RWPB_F_MSG_INIT(DtsTest_DtsTestRbacUser, user);
    
    msg = (ProtobufCMessage *)&user->base;
    ks = NULL;
    rw_keyspec_path_create_dup(((const rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUser)), NULL, &ks);
    rw_keyspec_path_set_category(ks, NULL, RW_SCHEMA_CATEGORY_DATA);
    userpath = (RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUser) *)ks;
    userpath->has_dompath = 1;
    userpath->dompath.path002.has_key00 = 1;
    userpath->dompath.path002.has_key01 = 1;
    userpath->dompath.path002.key00.user_name = strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUserConfig)*)keyspec)->dompath.path002.key00.user_name);
    userpath->dompath.path002.key01.user_domain= strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUserConfig)*)keyspec)->dompath.path002.key01.user_domain);

    user->user_name = strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUserConfig)*)keyspec)->dompath.path002.key00.user_name);
    user->user_domain = strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUserConfig)*)keyspec)->dompath.path002.key01.user_domain);
  }
  if (ks){
    status = rwdts_xact_block_add_query_ks(blk,
                                           ks,
                                           RWDTS_QUERY_CREATE,
                                           flags,  999, msg);
    ASSERT_EQ (status, RW_STATUS_SUCCESS);
    rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
    ASSERT_EQ (status, RW_STATUS_SUCCESS);
    rw_keyspec_path_free(ks, NULL);
    protobuf_c_message_free_unpacked(NULL, msg);
  }

  
  rwdts_appconf_prepare_complete_ok(ac, xact_info);
  
  rbac_test_completed_check(s);
}

static void
rbac_registration_regready(rwdts_member_reg_handle_t regh,
                      rw_status_t               rs,
                      void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  struct rbac_test_member_reg_data *regdata = rbac_find_reg_data(ti, regh);
  
  RW_ASSERT(regdata != NULL);
  regdata->happened.num_regready++;
  
  rbac_test_completed_check(s);
}

static rwdts_member_rsp_code_t
rbac_registration_abort(const rwdts_xact_info_t* xact_info,
                        uint32_t  n_crec,
                        const rwdts_commit_record_t** crec)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  struct rbac_test_member_reg_data *regdata = rbac_find_reg_data(ti, xact_info->regh);
  
  RW_ASSERT(regdata != NULL);
  regdata->happened.num_abort++;  
  rbac_test_completed_check(s);
  return RWDTS_ACTION_OK;
}


static rwdts_member_rsp_code_t
rbac_registration_commit(const rwdts_xact_info_t* xact_info,
                    uint32_t  n_crec,
                    const rwdts_commit_record_t** crec)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  struct rbac_test_member_reg_data *regdata = rbac_find_reg_data(ti, xact_info->regh);
  rwdts_member_cursor_t *cursor = NULL;
  RWPB_T_MSG(DtsTest_DtsTestRbacRole) *role;
  RWPB_T_MSG(DtsTest_DtsTestRbacUser) *user;
  rw_keyspec_path_t*       ks = NULL;
  
  RW_ASSERT(regdata != NULL);
  regdata->happened.num_commit++;

  if (!regdata->publish){
    if (regdata->keyspec->base.descriptor == RWPB_G_PATHSPEC_PBCMD(DtsTest_DtsTestRbacRole)){
      cursor = rwdts_member_data_get_cursor(NULL,
                                            regdata->regh);
      while ((role = (RWPB_T_MSG(DtsTest_DtsTestRbacRole) *)
              rwdts_member_reg_handle_get_next_element(regdata->regh,
                                                       cursor, NULL,
                                                       &ks)) != NULL){
        EXPECT_TRUE(strcmp(role->state_machine->state, "Active") == 0);
      }
      rwdts_member_data_delete_cursors(NULL, regdata->regh);
      
    }else  if (regdata->keyspec->base.descriptor == RWPB_G_PATHSPEC_PBCMD(DtsTest_DtsTestRbacUser)){
      cursor = rwdts_member_data_get_cursor(NULL,
                                            regdata->regh);
      while ((user = (RWPB_T_MSG(DtsTest_DtsTestRbacUser) *)
              rwdts_member_reg_handle_get_next_element(regdata->regh,
                                                       cursor, NULL,
                                                       &ks)) != NULL){
        EXPECT_TRUE(strcmp(user->state_machine->state, "Active") == 0);
      }
      rwdts_member_data_delete_cursors(NULL, regdata->regh);
    }
  }
  
  rbac_test_completed_check(s);
  return RWDTS_ACTION_OK;
}


static rwdts_member_rsp_code_t
rbac_registration_precommit(const rwdts_xact_info_t* xact_info,
                            uint32_t  n_crec,
                            const rwdts_commit_record_t** crec)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  struct rbac_test_member_reg_data *regdata = rbac_find_reg_data(ti, xact_info->regh);
  
  RW_ASSERT(regdata != NULL);
  regdata->happened.num_precommit++;
  
  rbac_test_completed_check(s);
  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rbac_registration_prepare(const rwdts_xact_info_t* xact_info,
                          RWDtsQueryAction         action,
                          const rw_keyspec_path_t* keyspec,
                          const ProtobufCMessage*  pbmsg,
                          uint32_t credits,
                          void*                    user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct rbac_test_data *s = (struct rbac_test_data *)ti->ctx;
  struct rbac_test_member_reg_data *regdata = rbac_find_reg_data(ti, xact_info->regh);
  
  RW_ASSERT(regdata != NULL);

  if (regdata->publish){
    switch(action){
      case RWDTS_QUERY_CREATE:
        break;
      case RWDTS_QUERY_UPDATE:
        {
          rwdts_xact_block_t *blk = NULL;
          uint32_t flags = RWDTS_XACT_FLAG_ADVISE | RWDTS_XACT_FLAG_END | RWDTS_XACT_FLAG_TRACE;
          ProtobufCMessage *msg = NULL;
          RWPB_T_MSG(DtsTest_DtsTestRbacRole) *role = NULL;
          RWPB_T_MSG(DtsTest_DtsTestRbacUser) *user = NULL;
          
          if (keyspec->base.descriptor == RWPB_G_PATHSPEC_PBCMD(DtsTest_DtsTestRbacRole)){
            if (strcmp(((RWPB_T_MSG(DtsTest_DtsTestRbacRole) *)pbmsg)->state_machine->state, "uagentgotit") == 0){
              msg =  protobuf_c_message_duplicate_allow_deltas(NULL,
                                                               (const ProtobufCMessage *)pbmsg,
                                                               pbmsg->descriptor);
              role = (RWPB_T_MSG(DtsTest_DtsTestRbacRole) *)msg;
              sprintf(role->state_machine->state, "Active");
            }
          }else if (keyspec->base.descriptor == RWPB_G_PATHSPEC_PBCMD(DtsTest_DtsTestRbacUser)){
            if (strcmp(((RWPB_T_MSG(DtsTest_DtsTestRbacUser) *)pbmsg)->state_machine->state, "uagentgotit") == 0){
              msg =  protobuf_c_message_duplicate_allow_deltas(NULL,
                                                               (const ProtobufCMessage *)pbmsg,
                                                               pbmsg->descriptor);
              
              
              user = (RWPB_T_MSG(DtsTest_DtsTestRbacUser) *)msg;
              sprintf(user->state_machine->state, "Active");
            }
          }
          if (msg){
            blk = rwdts_xact_block_create(xact_info->xact);
            rwdts_xact_block_add_query_ks(blk,
                                          keyspec,
                                          RWDTS_QUERY_UPDATE,
                                          flags,  999, msg);
            rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
            protobuf_c_message_free_unpacked(NULL, msg);
          }
        }
        break;
      default:
        break;
    }
  }else{
    if(regdata->retval.num_prepare[action] == UPDATE_ON_PREPARE_WITHOUT_ADVISE){
      rwdts_xact_block_t *blk = NULL;
      uint32_t flags = RWDTS_XACT_FLAG_END | RWDTS_XACT_FLAG_TRACE;
      rw_keyspec_path_t *ks = NULL;
      ProtobufCMessage *msg = NULL;
      
      blk = rwdts_xact_block_create(xact_info->xact);
      
      if (keyspec->base.descriptor == RWPB_G_PATHSPEC_PBCMD(DtsTest_DtsTestRbacRole)){
        RWPB_T_MSG(DtsTest_DtsTestRbacRole_StateMachine) *rolestate;
        RWPB_T_PATHSPEC(DtsTest_DtsTestRbacRole_StateMachine) *rolepath;
        rolestate = (RWPB_T_MSG(DtsTest_DtsTestRbacRole_StateMachine) *)RW_MALLOC0(sizeof(*rolestate));
        RWPB_F_MSG_INIT(DtsTest_DtsTestRbacRole_StateMachine, rolestate);
        msg = (ProtobufCMessage *)&rolestate->base;
        ks = NULL;
        rw_keyspec_path_create_dup(((const rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacRole_StateMachine)), NULL, &ks);
        rw_keyspec_path_set_category(ks, NULL, RW_SCHEMA_CATEGORY_DATA);
        rolepath =  (RWPB_T_PATHSPEC(DtsTest_DtsTestRbacRole_StateMachine) *)ks;
        rolepath->has_dompath = 1;
        rolepath->dompath.path002.has_key00 = 1;
        rolepath->dompath.path002.key00.role = strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacRole)*)keyspec)->dompath.path002.key00.role);
        rolestate->state = strdup("uagentgotit");
      }else if (keyspec->base.descriptor == RWPB_G_PATHSPEC_PBCMD(DtsTest_DtsTestRbacUser)){
        RWPB_T_MSG(DtsTest_DtsTestRbacUser_StateMachine) *userstate;
        RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUser_StateMachine) *userpath;
        userstate = (RWPB_T_MSG(DtsTest_DtsTestRbacUser_StateMachine) *)RW_MALLOC0(sizeof(*userstate));
        RWPB_F_MSG_INIT(DtsTest_DtsTestRbacUser_StateMachine, userstate);
        msg = (ProtobufCMessage *)&userstate->base;
        ks = NULL;
        rw_keyspec_path_create_dup(((const rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUser_StateMachine)), NULL, &ks);
        rw_keyspec_path_set_category(ks, NULL, RW_SCHEMA_CATEGORY_DATA);
        userpath =  (RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUser_StateMachine) *)ks;
        userpath->has_dompath = 1;
        userpath->dompath.path002.has_key00 = 1;
        userpath->dompath.path002.has_key01 = 1;
        userpath->dompath.path002.key00.user_name = strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUser)*)keyspec)->dompath.path002.key00.user_name);
        userpath->dompath.path002.key01.user_domain = strdup(((RWPB_T_PATHSPEC(DtsTest_DtsTestRbacUser)*)keyspec)->dompath.path002.key01.user_domain);
        
        userstate->state = strdup("uagentgotit");
      }
      if (ks){
        rwdts_xact_block_add_query_ks(blk,
                                      ks,
                                      RWDTS_QUERY_UPDATE,
                                      flags,  999, msg);
        //ASSERT_EQ (status, RW_STATUS_SUCCESS);
        rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
        //ASSERT_EQ (status, RW_STATUS_SUCCESS);
        rw_keyspec_path_free(ks, NULL);
        protobuf_c_message_free_unpacked(NULL, msg);
      }
    }
  }
  
  regdata->happened.num_prepare[action]++;
  rbac_test_completed_check(s);
  return RWDTS_ACTION_OK;
}


static void
rbac_clnt_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  
}







struct rbac_test_member_reg_data*
rbac_member_register_config(struct rbac_test_data *s, twhich_t which,
                            rw_keyspec_path_t *keyspec,
                            const ProtobufCMessageDescriptor *pbdesc,
                            rwdts_appconf_prepare_cb_t prepare_cb,
                            struct rbac_test_member_appconf **myappconf)
{
  rwdts_appconf_cbset_t cbset;
  struct tenv1::tenv_info *ti = NULL;
  uint32_t flags = ( RWDTS_FLAG_SUBSCRIBER | RWDTS_FLAG_CACHE );
  struct rbac_test_member_reg_data *ret;
  rw_keyspec_path_t *ks;
  
  ks = NULL;
  rw_keyspec_path_create_dup((const rw_keyspec_path_t*)keyspec, NULL, &ks);
  rw_keyspec_path_set_category(ks, NULL, RW_SCHEMA_CATEGORY_CONFIG);

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
    
  ti = &s->tenv->t[which];
  if (!*myappconf){
    memset(&cbset, 0, sizeof(cbset));
    cbset.xact_init = rbac_xact_init;
    cbset.xact_deinit = rbac_xact_deinit;
    cbset.config_validate = rbac_validate;
    cbset.config_apply = rbac_apply;
    cbset.ctx = ti;
    //&s->member[which].appconf[s->member[which].num_appconf];

    *myappconf = &s->member[which].appconf[s->member[which].num_appconf];
    (*myappconf)->ac =
        rwdts_appconf_group_create(ti->apih, NULL, &cbset);
    s->member[which].num_appconf++;
  }
  ret = &(*myappconf)->reg[(*myappconf)->num_reg];
  memset(&(*myappconf)->reg[(*myappconf)->num_reg], 0, sizeof((*myappconf)->reg[(*myappconf)->num_reg]));
  (*myappconf)->reg[(*myappconf)->num_reg].regh = rwdts_appconf_register((*myappconf)->ac,
                                                                         ks, NULL, /*shard*/
                                                                         pbdesc, flags, prepare_cb);
  (*myappconf)->num_reg++;

  rw_keyspec_path_free(ks, NULL);
  
  return ret;
}


static struct rbac_test_member_reg_data*
rbac_add_member_registration(struct rbac_test_data *s, twhich_t which,
                             int flags,
                             rw_keyspec_path_t*               ks,
                             const ProtobufCMessageDescriptor *pbdesc)
{
  rwdts_member_event_cb_t reg_cb;
  struct tenv1::tenv_info *ti = NULL;
  struct rbac_test_member_reg_data *ret;
  rw_keyspec_path_t*               keyspec;
  keyspec = NULL;
  rw_keyspec_path_create_dup((const rw_keyspec_path_t*)ks, NULL, &keyspec);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  
  ti = &s->tenv->t[which];
  
  memset(&reg_cb, 0, sizeof(reg_cb));
  reg_cb.ud = ti;
  
  reg_cb.cb.reg_ready = rbac_registration_regready;
  reg_cb.cb.prepare = rbac_registration_prepare;
  reg_cb.cb.precommit = rbac_registration_precommit;
  reg_cb.cb.commit = rbac_registration_commit;
  reg_cb.cb.abort = rbac_registration_abort;
  
  ret = &s->member[which].reg[s->member[which].num_reg];
  memset(ret, 0,
         sizeof(*ret));
  if (flags & RWDTS_FLAG_PUBLISHER){
    ret->publish = true;
  }else{
    ret->publish = false;
  }
  ret->expected.num_regready = 1;
  ret->regh = rwdts_member_register(NULL,
                                    ti->apih,
                                    keyspec,
                                    &reg_cb,
                                    pbdesc,
                                    flags,
                                    NULL);
  s->member[which].num_reg++;
  
  rw_keyspec_path_free(keyspec, NULL);
  
  return ret;
}

static void
rbac_start_a_config_xact(struct rbac_test_data *s)
{
  rw_keyspec_path_t*               keyspec;
  struct tenv1::tenv_info *ti = NULL;
  RWPB_M_MSG_DECL_INIT(DtsTest_DtsTestRbacConfig, config);
  RWPB_M_MSG_DECL_INIT(DtsTest_DtsTestRbacRoleConfig, role);
  RWPB_M_MSG_DECL_INIT(DtsTest_DtsTestRbacUserConfig, user);
  RWPB_T_MSG(DtsTest_DtsTestRbacRoleConfig) *role_array[1];
  RWPB_T_MSG(DtsTest_DtsTestRbacUserConfig) *user_array[1];
  char rolename[64];
  char username[64];
  char user_domain[64];
  
  config.n_role_definition = 1;
  config.role_definition = role_array;
  role_array[0] = &role;
  
  config.n_users = 1;
  
  user_array[0] = &user;
  config.users = user_array;
  role.role = rolename;
  user.user_name = username;
  user.user_domain = user_domain;
  sprintf(username, "Riftuser");
  sprintf(user_domain, "riftio.com");
  sprintf(rolename, "admin");
  
  ti = &s->tenv->t[TASKLET_CLIENT];
  rwdts_xact_t* xact = NULL;
  rwdts_xact_block_t *blk = NULL;
  uint32_t flags = RWDTS_XACT_FLAG_END | RWDTS_XACT_FLAG_ADVISE | RWDTS_XACT_FLAG_TRACE;
  rw_status_t status;
  
  
  xact = rwdts_api_xact_create(ti->apih, flags, rbac_clnt_query_callback, ti);
  ASSERT_TRUE(xact);
  blk = rwdts_xact_block_create(xact);
  ASSERT_TRUE(blk);
  keyspec = NULL;
  rw_keyspec_path_create_dup(((const rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacConfig)), NULL, &keyspec);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  status = rwdts_xact_block_add_query_ks(blk,
                                         keyspec,
                                           RWDTS_QUERY_CREATE,
                                         flags,  1000, &config.base);
  ASSERT_EQ (status, RW_STATUS_SUCCESS);
  rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
  ASSERT_EQ (status, RW_STATUS_SUCCESS);
  rw_keyspec_path_free(keyspec, NULL); 
}



TEST_F(RWDtsEnsemble, TestRbacConfig)
{
  struct rbac_test_data s = {};
  struct rbac_test_member_appconf *appconf;
  struct rbac_test_member_reg_data*reg;

  s.exitnow = 0;
  
  /*reusing the initialize*/
  init_test_state(&s);
  init_test_member_state(&s, TASKLET_ROUTER);
  init_test_member_state(&s, TASKLET_BROKER);
  init_test_member_state(&s, TASKLET_CLIENT);
  init_test_member_state(&s, TASKLET_MEMBER_0);
  init_test_member_state(&s, TASKLET_MEMBER_1);
  init_test_member_state(&s, TASKLET_MEMBER_2);
  
  /*Go through all the members and finish the registrations.*/
  appconf = NULL;
  reg = rbac_member_register_config(&s, TASKLET_MEMBER_0,
                                    (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacRoleConfig)),
                                    (const ProtobufCMessageDescriptor *)RWPB_G_MSG_PBCMD(DtsTest_DtsTestRbacRoleConfig),
                                    member_config_prepare, &appconf);
  reg->expected  = {0, {0,0,0,0,0,0}, 0, 0, 0};
  
  /* Say we're finished */
  rwdts_appconf_phase_complete(appconf->ac, RWDTS_APPCONF_PHASE_REGISTER);
    
  
  /*Publisher of role and subscriber for user*/
  reg = rbac_add_member_registration(&s,  TASKLET_MEMBER_0,
                                     RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST|RWDTS_FLAG_SHARED,
                                     (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacRole)),
                                     RWPB_G_MSG_PBCMD(DtsTest_DtsTestRbacRole));
  rw_keyspec_path_create_dup((const rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacRole)), NULL,
                             &reg->keyspec);
  reg->expected  = {1, {0,0,0,1,0,0}, 0, 0, 0};

  reg = rbac_add_member_registration(&s, TASKLET_MEMBER_0,
                                     RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE,
                                     (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUser)),
                                     RWPB_G_MSG_PBCMD(DtsTest_DtsTestRbacUser));
  rw_keyspec_path_create_dup((const rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUser)), NULL,
                             &reg->keyspec);
  reg->expected  = {1, {0,1,0,1,0,0}, 1, 1, 0};

  
  appconf =  NULL;
  reg = rbac_member_register_config(&s, TASKLET_MEMBER_1,
                                    (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUserConfig)),
                                    RWPB_G_MSG_PBCMD(DtsTest_DtsTestRbacUserConfig),
                                    member_config_prepare, &appconf);
  reg->expected  = {0, {0,0,0,0,0,0}, 0, 0, 0};
  /* Say we're finished */
  rwdts_appconf_phase_complete(appconf->ac, RWDTS_APPCONF_PHASE_REGISTER);
  
  reg = rbac_add_member_registration(&s, TASKLET_MEMBER_1,
                                     RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST|RWDTS_FLAG_SHARED,
                                     (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUser)),  RWPB_G_MSG_PBCMD(DtsTest_DtsTestRbacUser));
  rw_keyspec_path_create_dup((const rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUser)), NULL,
                             &reg->keyspec);
  reg->expected  = {1, {0,0,0,1,0,0}, 0, 0, 0};
  


  reg = rbac_add_member_registration(&s, TASKLET_MEMBER_2,
                                     RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE,
                                     (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUser)),
                                     RWPB_G_MSG_PBCMD(DtsTest_DtsTestRbacUser));
  rw_keyspec_path_create_dup((const rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacUser)), NULL,
                             &reg->keyspec);
  reg->expected  = {1, {0,1,0,1,0,0}, 1, 1, 0};
  reg->retval = {0, {0,UPDATE_ON_PREPARE_WITHOUT_ADVISE,0,0,0}, 0, 0, 0};
  
  reg = rbac_add_member_registration(&s, TASKLET_MEMBER_2,
                                     RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE,
                                     (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacRole)),
                                     RWPB_G_MSG_PBCMD(DtsTest_DtsTestRbacRole));
  rw_keyspec_path_create_dup((const rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_DtsTestRbacRole)), NULL,
                             &reg->keyspec);
  reg->expected  = {1, {0,1,0,1,0,0}, 1, 1, 0};
  reg->retval = {0, {0,UPDATE_ON_PREPARE_WITHOUT_ADVISE,0,0,0}, 0, 0, 0};
  
  /*Wait for the registrations to become ready in the router*/
  rwsched_dispatch_main_until(s.tenv->t[0].tasklet, 30, &s.exitnow);
  s.exitnow = 0;
  
  /*start a xact config the*/
  rbac_start_a_config_xact(&s);
  
  double seconds = (RUNNING_ON_VALGRIND)?240:120;
  rwsched_dispatch_main_until(s.tenv->t[0].tasklet, seconds, &s.exitnow);
  
  ASSERT_TRUE(s.exitnow);
  
  memset(&s, 0xaa, sizeof(s));
}



