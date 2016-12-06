
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
 * @file rwuagent_dom_map.cpp
 *
 * Management agent confd hkeypath to DOM mapping support.
 */

#include "rwuagent.hpp"
#include <sstream>

using namespace rw_uagent;
using namespace rw_yang;
using namespace std::chrono;


std::string NodeMap::debug_print()
{
  std::ostringstream oss;
  for (auto& elem : keypath_set_) {
    oss << elem << '\n';
  }
  return oss.str();
}

std::string OperationalDomMap::debug_print()
{
  std::ostringstream oss;
  for (auto& elem : list_ctxt_map_) {
    oss << elem.first << ':' << elem.second << '\n';
    auto n = node(elem.second);
    if (n) oss << n.get().debug_print() << '\n';
  }
  return oss.str();
}

void OperationalDomMap::add_dom(
  rw_yang::XMLDocument::uptr_t&& dom_ptr,
  const char* keypath)
{
  RW_ASSERT(keypath);

  //Erase old dom, if exists
  remove_dom(this->dom(keypath));

  dom_counter_++;

  dom_counter_map_.emplace(dcm_value_type(dom_ptr.get(), dom_counter_));
  dom_map_.emplace(dm_value_type(dom_counter_,
                                 NodeMap(std::move(dom_ptr),
                                         keypath))
                  );

  list_ctxt_map_.emplace(lcm_value_type(
                                  std::string(keypath),
                                  dom_counter_));
}

rw_yang::XMLDocument*
OperationalDomMap::dom(const char* keypath) const noexcept
{
  if(nullptr == keypath) return nullptr;
  return dom(dom_counter(keypath));
}

rw_yang::XMLDocument* OperationalDomMap::dom(
  const confd_hkeypath_t *keypath) const
{
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof(confd_hkeypath_buffer) - 1, keypath);

  return dom(confd_hkeypath_buffer);
}

bool OperationalDomMap::remove_dom_if_expired(
  const confd_hkeypath_t *keypath,
  uint32_t dom_expiry_period_ms)
{
  auto node_map = node(keypath);
  if (!node_map) {
    return true;
  }
  clock_t::time_point curr = clock_t::now();
  milliseconds diff = duration_cast<milliseconds>
                (curr - node_map.get().creation_time_);

  if (diff.count() >= dom_expiry_period_ms) {
    remove_dom(node_map.get().doc_.get());
    return true;
  }
  return false;
}

void OperationalDomMap::cleanup_if_expired(
  rw_confd_client_type_t type,
  uint32_t cli_dom_refresh_period_ms,
  uint32_t nc_rest_dom_refresh_period_ms)
{
  int expiry_time = 0;

  switch (type) {
  case CLI:
    expiry_time = cli_dom_refresh_period_ms;
    break;
  case RESTCONF:
  case NETCONF:
    expiry_time = nc_rest_dom_refresh_period_ms;
    break;
  default:
    RW_ASSERT_NOT_REACHED();
  };

  clock_t::time_point curr = clock_t::now();

  auto it_beg = dom_map_.begin();
  auto it_end = dom_map_.end();

  while (it_beg != it_end) {
    auto it = it_beg++;
    milliseconds diff = duration_cast<milliseconds>
            (curr - it->second.creation_time_);

    if (diff.count() >= expiry_time) {
      remove_dom(it->second.doc_.get());
    }
  }
}

