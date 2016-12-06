
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
 * @file rw_fsm.c
 * @author Vinod Kamalaraj vinod.kamalaraj@riftio.com)
 * @date 03/13/2013
 * @brief RIFT FSM utilities
 * 
 * @details These utilities are used to implement protocol FSMs
 */


#include "rw_fsm.h"

/**
 * Record an event that occured in a FSM
 *
 *  @param[in] event - The event being recorded
 *  @param[in] old_state - The state being recorded
 *  @param[inout] trace - Trace record where to record the event
 *
 *  @returns void
 */
void rw_record_event (rw_fsm_event_t *event,
                      uint8_t        old_state,
                      rw_fsm_trace_t *trace) {  
  rw_fsm_trace_element_t *elt;
  RW_ASSERT(event);
  RW_ASSERT(trace);

  elt = &trace->rft_elts[trace->rft_curr_idx];

  elt->rft_state = old_state;
  elt->rft_mapped_event = event->rfe_type;
  elt->rft_protocol = event->rfe_protocol;
  /// Add timestamp

  trace->rft_curr_idx = (trace->rft_curr_idx + 1) % RW_FSM_MAX_TRACE_ELTS;
}



/**
 * Handle an event that occured in the FSM instance
 *
 * @param[in] fsm - The FSM instance
 * @param[in] hndl - ???
 * @param[in] protocol - protocol related to the event ???
 * @param[in] glob - ???
 * @param[in] prot_event - The protocol event to be handled
 * @param[out] ret - FSM error return code for the event
 *
 * @returns RW_STATUS_SUCCESS upon success
 * @returns RW_STATUS_FAILURE (or other error codes) on failure
 */
rw_status_t rw_fsm_handle_event (rw_fsm_t              *fsm,      
				 rw_fsm_handle_t       hndl,      
				 rw_event_protocol_t   protocol,  
				 rw_fsm_global_data_t  *glob,     
				 void                  *prot_event,
				 rw_fsm_eh_return_t    *ret) 
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rw_fsm_event_t event;
  uint8_t old_state = 0, new_state = 0;
  rw_fsm_state_data_t *state = 0;
  rw_fsm_eh_t fn = 0;
  RW_ASSERT(fsm);
  RW_ASSERT(hndl);
  //RW_ASSERT(glob);
  RW_ASSERT(prot_event);
  RW_ASSERT(ret);
  
  RW_ZERO_VARIABLE (&event);
  event.rfe_type = 0;
  event.rfe_event = prot_event;
  event.rfe_protocol = protocol;

  /// - map the protocol event to the fsm event
  RW_ASSERT(fsm->rfs_event_map);
  status = fsm->rfs_event_map (&event);

  /// - find the state information from the handle

  state = (rw_fsm_state_data_t *) ((char *)hndl + fsm->rfs_offset);
  RW_ASSERT (!state->rsd_in_eh);
  old_state = state->rsd_state;
  
  /// Add the event to the fsm trace - even if it failed mapping
  rw_record_event (&event, old_state, &state->rsd_trace);

  if (status != RW_STATUS_SUCCESS) {
    // Add logs
    return status;
  }

  RW_ASSERT (event.rfe_type < fsm->rfs_event_count);
  
  fn = fsm->rfs_eh[old_state * fsm->rfs_event_count + event.rfe_type];
  if (!fn) {
    fn = fsm->rfs_unexp_eh;
  }

  if (!fn) {
    /// If there is no event handler, and no unexpected event handler.
    /// Log and return
    ret->rer_ev_disposition = RW_EVENT_DISPOSITION_IGNORED;
    return RW_STATUS_FAILURE;
  }

  state->rsd_in_eh = 1;

  /// Stop any running timers for the old state
  /// Decrement stats in previous state
  RW_ASSERT(fn);  
  status = fn (hndl, &event, glob, ret);

  if (RW_HANDLE_DISPOSITION_DESTROYED == ret->rer_handle_disposition) {
    /// Log and return
    return status;
  }

  state->rsd_in_eh = 0;
  state = (rw_fsm_state_data_t *) ((char *)hndl + fsm->rfs_offset);
  new_state = state->rsd_state;

  if (new_state != old_state) {
    if (fsm->rfs_state_prfl[old_state].exit) {
      fsm->rfs_state_prfl[old_state].exit (hndl, old_state, 0);
    }
    if (fsm->rfs_state_prfl[new_state].entry) {
      fsm->rfs_state_prfl[new_state].entry (hndl, new_state, 1);
    }    
  }

  /// Start timers associated with new state
  /// Increment timers associated with new state

  return status;
}
