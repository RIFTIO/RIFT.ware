
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
 * @file rw_fsm_test.c
 * @author Vinod Kamalaraj
 * @date 03/10/2014
 * @brief Test routine for testing FSM frame work
 * @details Test FSM framework
 */

#include <limits.h>
#include <cstdlib>
#include "rwut.h"
#include "rw_fsm.h"

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;


#define GTEST_ON

int RwFSMPositiveCases();
int RwFSMNegativeCases();

typedef struct {
  int message_type;
} my_test_msg_type;

typedef struct {
  uint8_t state_entry_called[3];
  uint8_t state_exit_called[3];

  uint8_t exp_state_event_matrix[3][3];
  rw_fsm_state_data_t state;
  uint8_t unexp_state_event_matrix[3][3];
  
} fsm_test_handle_t;

extern rw_fsm_t test_fsm, test_fsm_no_unexp_event_handler;

// Entry and exit funcs
void fsm_entry_exit_func (rw_fsm_handle_t hndl, uint8_t state, uint8_t is_enter)
{
  fsm_test_handle_t *handle = (fsm_test_handle_t *)hndl;
  
  RW_ASSERT (state < 3);
  if (is_enter)
    handle->state_entry_called[state]++;
  else
    handle->state_exit_called[state]++;
}

rw_status_t my_map_func (rw_fsm_event_t *event) {
  my_test_msg_type *msg = (my_test_msg_type *) event->rfe_event;
  if (msg->message_type < 4) {
    event->rfe_type = msg->message_type;
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;
}

rw_status_t my_exp_event_handler (rw_fsm_handle_t hndl,
                                    rw_fsm_event_t  *event,
                                    rw_fsm_global_data_t *glob,
                                    rw_fsm_eh_return_t *ret) {
  
  fsm_test_handle_t *handle = (fsm_test_handle_t *)hndl;
  rw_status_t status;
  UNUSED (glob);
  handle->exp_state_event_matrix[handle->state.rsd_state][event->rfe_type]++;

  if (handle->state.rsd_state != 2) {
    handle->state.rsd_state = event->rfe_type;
    return RW_STATUS_SUCCESS;
  }

  switch (event->rfe_type) {
    case 2:
      handle->state.rsd_state = 0;
      ret->rer_handle_disposition = RW_HANDLE_DISPOSITION_DESTROYED;
      return RW_STATUS_SUCCESS;
    case 1:
      handle->state.rsd_state = event->rfe_type;
      return RW_STATUS_SUCCESS;
    case 0:
      status = rw_fsm_handle_event (&test_fsm, hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, event->rfe_event, ret);
      return status;
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t re_enter_fsm (rw_fsm_handle_t hndl,
                          rw_fsm_event_t  *event,
                          rw_fsm_global_data_t *glob,
                          rw_fsm_eh_return_t *ret) {

  my_test_msg_type msg;
  rw_status_t status;
  UNUSED (event);
  UNUSED (glob);
  
  msg.message_type = 2;
  status = rw_fsm_handle_event (&test_fsm, hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, ret);
  return status;
}

rw_fsm_eh_t my_eh_list[9] = {
  my_exp_event_handler,
  0,
  my_exp_event_handler,
  0,
  re_enter_fsm,
  my_exp_event_handler,
  my_exp_event_handler,
  my_exp_event_handler,
  my_exp_event_handler
};

rw_status_t my_unexp_event_handler (rw_fsm_handle_t hndl,
                                      rw_fsm_event_t  *event,
                                      rw_fsm_global_data_t *glob,
                                      rw_fsm_eh_return_t *ret) {
  
  fsm_test_handle_t *handle = (fsm_test_handle_t *)hndl;
  UNUSED (glob);
  UNUSED (ret);
  handle->unexp_state_event_matrix[handle->state.rsd_state][event->rfe_type]++;

  return RW_STATUS_SUCCESS;
}

  
// Define a couple of FSMs
rw_fsm_state_profile_t fsm_1_states[] = {
  { "FSM_1_STATE_1", 0, 0, fsm_entry_exit_func, fsm_entry_exit_func},
  { "FSM_1_STATE_2", 1, 2, fsm_entry_exit_func, 0 },
  { "FSM_1_STATE_3", 2, 3, 0, 0}
};

rw_fsm_event_profile_t fsm_1_events[] = {
  { "EVENT_1", 0},
  { "EVENT_2", 0},
  { "EVENT_3", 0},
};

rw_fsm_t test_fsm = {
  3,       /*< Number of states in the FSM */
  3,       /*< Number of events in the FSM */
  offsetof (fsm_test_handle_t, state),            /*< Offset within handle of state data */
  fsm_1_states,        /*< Profiles of the states */
  fsm_1_events,        /*< Profiles of the events */
  0,        /*< Map timer enums to values */
  my_map_func,         /*< Get FSM enums for current event */
  my_eh_list,               /*< Event handlers for expected events */
  my_unexp_event_handler,        /*< All unexpected events */
};

rw_fsm_t test_fsm_no_unexp_event_handler = {
  3,       /*< Number of states in the FSM */
  3,       /*< Number of events in the FSM */
  offsetof (fsm_test_handle_t, state),            /*< Offset within handle of state data */
  fsm_1_states,        /*< Profiles of the states */
  fsm_1_events,        /*< Profiles of the events */
  0,        /*< Map timer enums to values */
  my_map_func,         /*< Get FSM enums for current event */
  my_eh_list,               /*< Event handlers for expected events */
  NULL,        /*< All unexpected events */
};

void
mySetup(int argc, char** argv)
{

  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  RW_ZERO_VARIABLE(&hndl);

  hndl.state.rsd_state = 2;
  msg.message_type = 2;
  rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);

  if (argc == 1)
    {
      // Yes this is cheating on argv memory, but oh well...
      argv[1] = (char *) "--gtest_catch_exceptions=0";
      argc++;
    }

  {
    pthread_t self_id;
    self_id=pthread_self();
    printf("\nMain thread %lu\n",self_id);
  }

#ifndef GTEST_ON
  printf("calling RwFSMPositiveCases\n");
  RwFSMPositiveCases();

  printf("calling RwFSMNegativeCases\n");
  RwFSMNegativeCases();
#endif
}
RWUT_INIT(mySetup);

#ifndef GTEST_ON
int RwFSMPositiveCases() {
#define ASSERT_EQ(...)   
#else
TEST(RwFSM, RwFSMPositiveExpectedNoStateChange) {
  TEST_DESCRIPTION("Expected Event No state change"); 
#endif
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  rw_status_t status;
  RW_ZERO_VARIABLE(&hndl);

  msg.message_type = 0;
  status = rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
  ASSERT_EQ (RW_STATUS_SUCCESS, status);
  // State 0, event 0 - expected - no state change
  ASSERT_EQ (hndl.state_entry_called[0], 0);
  ASSERT_EQ (hndl.state_exit_called[0], 0);
  ASSERT_EQ (hndl.exp_state_event_matrix[0][0], 1);
  ASSERT_EQ (hndl.state.rsd_trace.rft_curr_idx, 1);
  ASSERT_EQ (hndl.state.rsd_trace.rft_elts[0].rft_state, 0);
  ASSERT_EQ (hndl.state.rsd_trace.rft_elts[0].rft_mapped_event, 0);

}

TEST(RwFSM, RwFSMPositiveUnExpected) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  rw_status_t status;
  RW_ZERO_VARIABLE(&hndl);


  TEST_DESCRIPTION("Unexpected Event"); 

  // State 0, event 1 - not expected - no state change
  msg.message_type = 1;
  status = rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
  ASSERT_EQ (RW_STATUS_SUCCESS, status);
  ASSERT_EQ (hndl.state_entry_called[0], 0);
  ASSERT_EQ (hndl.state_exit_called[0], 0);
  ASSERT_EQ (hndl.unexp_state_event_matrix[0][1], 1);
  ASSERT_EQ (hndl.state.rsd_trace.rft_curr_idx, 1);
  ASSERT_EQ (hndl.state.rsd_trace.rft_elts[0].rft_state, 0);
  ASSERT_EQ (hndl.state.rsd_trace.rft_elts[0].rft_mapped_event, 1);

  // State 0, event 1 - not expected - no state change - no unexpected event handler available
  msg.message_type = 1;
  status = rw_fsm_handle_event (&test_fsm_no_unexp_event_handler, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
  ASSERT_EQ (RW_STATUS_FAILURE, status);
  ASSERT_EQ (hndl.state_entry_called[0], 0);
  ASSERT_EQ (hndl.state_exit_called[0], 0);
  ASSERT_EQ (hndl.unexp_state_event_matrix[0][1], 1);
  ASSERT_EQ (hndl.state.rsd_trace.rft_curr_idx, 2);
  ASSERT_EQ (hndl.state.rsd_trace.rft_elts[0].rft_state, 0);
  ASSERT_EQ (hndl.state.rsd_trace.rft_elts[0].rft_mapped_event, 1);

}

TEST(RwFSM, RwFSMPositiveExpectedStateChange) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  rw_status_t status;
  RW_ZERO_VARIABLE(&hndl);


  TEST_DESCRIPTION("Expected Event-State Change + Exit function called"); 
  
  // State 0, event 2 -  expected -  state change to 2
  msg.message_type = 2;
  status = rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
  ASSERT_EQ (RW_STATUS_SUCCESS, status);
  ASSERT_EQ (hndl.state_entry_called[0], 0);
  ASSERT_EQ (hndl.state_exit_called[0], 1);
  ASSERT_EQ (hndl.exp_state_event_matrix[0][0], 0);
  ASSERT_EQ (hndl.unexp_state_event_matrix[0][1], 0);
  ASSERT_EQ (hndl.exp_state_event_matrix[0][2], 1);
  ASSERT_EQ (hndl.state.rsd_trace.rft_curr_idx, 1);
}

TEST(RwFSM, RwFSMPositiveExpectedStateChangeEntryFnCheck) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  rw_status_t status;
  RW_ZERO_VARIABLE(&hndl);


  TEST_DESCRIPTION("Expected Event-State Change + Entry function called"); 

  // State 2, event 1 -  expected -  state change to 1
  hndl.state.rsd_state = 2;
  msg.message_type = 1;
  status = rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
  ASSERT_EQ (RW_STATUS_SUCCESS, status);
  ASSERT_EQ (hndl.state_entry_called[1], 1);
  ASSERT_EQ (hndl.exp_state_event_matrix[2][1], 1);
  ASSERT_EQ (hndl.state.rsd_trace.rft_curr_idx, 1);
}

TEST(RwFSM, RwFSMPositiveTraceIndexRollOver) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  int i = 0;
  rw_status_t status;  
  RW_ZERO_VARIABLE(&hndl);


  TEST_DESCRIPTION("Trace Index rollover"); 
  
  msg.message_type = 1;
  for (i = 0; i < 11; i++) {
    status = rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
    ASSERT_EQ (RW_STATUS_SUCCESS, status);
  }

  ASSERT_EQ (hndl.state.rsd_trace.rft_curr_idx, 1);
}

TEST(RwFSM, RwFSMPositiveExpectedContextDestroyed) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  rw_status_t status;
  RW_ZERO_VARIABLE(&hndl);


  TEST_DESCRIPTION("Trace Index rollover"); 

  // State 2, event 2 -  expected, state should change to 0, but entry fn not called, since
  // disposition is returned as context destroyed.
  hndl.state.rsd_state = 2;
  msg.message_type = 2;
  status = rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
  ASSERT_EQ (RW_STATUS_SUCCESS, status);
  ASSERT_EQ (hndl.state_entry_called[0], 0);
  ASSERT_EQ (hndl.state.rsd_state , 0);
  ASSERT_EQ (hndl.state.rsd_in_eh, 1);
}

TEST(RwFSM, RwFSMPositiveMapFailure) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  rw_status_t status;
  RW_ZERO_VARIABLE(&hndl);

  hndl.state.rsd_state = 2;
  msg.message_type = 5;
  status = rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
  ASSERT_EQ (RW_STATUS_FAILURE, status);
  ASSERT_EQ (hndl.state.rsd_trace.rft_curr_idx, 1);
  
}

#if 0
TEST(RwFSMNegative, ReentryFake) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  rw_status_t status;
  RW_ZERO_VARIABLE(&hndl);

  hndl.state.rsd_state = 2;
  msg.message_type = 2;
  status = rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);
  ASSERT_EQ (RW_STATUS_SUCCESS, status);
  EXPECT_DEATH({rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);}, "");

}

TEST(RwFSMNegative, ReentryReal) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  RW_ZERO_VARIABLE(&hndl);

  hndl.state.rsd_state = 1;
  msg.message_type = 1;

  EXPECT_DEATH({rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);}, "");
}

TEST(RwFSMNegative, MappedEnumTooHigh) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  RW_ZERO_VARIABLE(&hndl);

  hndl.state.rsd_state = 1;
  msg.message_type = 3;
  EXPECT_DEATH({rw_fsm_handle_event (&test_fsm, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);}, "");
}


TEST(RwFSMNegative, NoMapFunction) {
  fsm_test_handle_t hndl;
  my_test_msg_type msg;
  rw_fsm_eh_return_t ret;
  rw_fsm_t tmp = test_fsm;
  RW_ZERO_VARIABLE(&hndl);
  UNUSED (tmp);
  
  tmp.rfs_event_map = 0;
  hndl.state.rsd_state = 1;
  msg.message_type = 1;
  EXPECT_DEATH({rw_fsm_handle_event (&tmp, &hndl, RW_EVENT_PROTOCOL_INTERNAL_TEST, (rw_fsm_global_data_t *) 1, &msg, &ret);}, "");
}

#endif
