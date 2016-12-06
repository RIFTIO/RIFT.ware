
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
 * @file rwuagent_confd_dom_stats.cpp
 *
 * Management agent confd-XML DOM usage statistics.
 */

#include "rwuagent.hpp"
#include "rw-mgmtagt-confd.pb-c.h"

using namespace rw_uagent;
using namespace rw_yang;


ConfdDomStats::ConfdDomStats(
  const char *hkeypath,
  const char *dts_key )
{
  RW_ASSERT(hkeypath);
  RW_ASSERT(dts_key);

  RWPB_F_MSG_INIT (RwMgmtagtConfd_CachedDom, &dom_params_);

  dom_params_.path = strdup (hkeypath);
  dom_params_.dts_keyspec = strdup (dts_key);
  gettimeofday (&create_time_, nullptr);
  memset (&last_access_time_, 0, sizeof (last_access_time_));
}

ConfdDomStats::~ConfdDomStats()
{
  if (dom_params_.n_callbacks) {
    RW_ASSERT (dom_params_.callbacks);
    RW_FREE (dom_params_.callbacks);
    dom_params_.n_callbacks = 0;
  }

  for (auto it : callbacks_) {
    protobuf_c_message_free_unpacked(nullptr,(ProtobufCMessage *)it.second);
  }
}

RWPB_T_MSG(RwMgmtagtConfd_CachedDom)* ConfdDomStats::get_pbcm()
{

  if (dom_params_.n_callbacks) {
    if (dom_params_.callbacks) {
      RW_FREE (dom_params_.callbacks);
    }
    dom_params_.n_callbacks = 0;
  }

  dom_params_.n_callbacks = callbacks_.size();
  if (dom_params_.n_callbacks) {
    dom_params_.callbacks =  (RWPB_T_MSG(RwMgmtagtConfd_Callbacks) **)
        RW_MALLOC (sizeof (RWPB_T_MSG(RwMgmtagtConfd_Callbacks) **) * dom_params_.n_callbacks);
  }

  size_t i = 0;
  for (auto it : callbacks_) {
    dom_params_.callbacks[i++] = it.second;
  }
  // Find the elapsed time
  struct timeval elapsed;
  RW_ASSERT(timercmp(&last_access_time_, &create_time_, >=));
  timersub(&last_access_time_,&create_time_,&elapsed);
  dom_params_.has_usage_time = 1;
  dom_params_.usage_time = elapsed.tv_sec * 1000000 + elapsed.tv_usec;

  return &dom_params_;
}

void ConfdDomStats::log_state (RwMgmtagt_Confd_CallbackType type)
{
  gettimeofday (&last_access_time_, nullptr);

  if (!dom_params_.has_create_time) {
    // Only create had been called. Set the create time.
    struct timeval elapsed;
    RW_ASSERT(timercmp(&last_access_time_, &create_time_, >=));
    timersub(&last_access_time_,&create_time_,&elapsed);
    dom_params_.has_create_time = 1;
    dom_params_.create_time = elapsed.tv_sec * 1000000 + elapsed.tv_usec;
    // Reset create time to curr time so that the usage time is after this
    create_time_ = last_access_time_;
  }

  auto cb = callbacks_.find(type);
  if (callbacks_.end() != cb) {
    cb->second->count ++;
  } else {
    RWPB_T_MSG (RwMgmtagtConfd_Callbacks) *callback = (RWPB_T_MSG (RwMgmtagtConfd_Callbacks) *)
        protobuf_c_message_create(nullptr, RWPB_G_MSG_PBCMD(RwMgmtagtConfd_Callbacks));

    callback->type = type;
    callback->has_type = 1;
    callback->has_count = 1;
    callback->count = 1;

    callbacks_[type] = callback;
  }
}

