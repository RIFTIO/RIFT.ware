
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


/*!
 * @file rwuagent_request_mode.hpp
 *
 * Management agent southbound request type;
 */

#ifndef CORE_MGMT_RWUAGENT_REQUEST_MODE_HPP_
#define CORE_MGMT_RWUAGENT_REQUEST_MODE_HPP_

namespace rw_uagent {

/*!
 * 
 */
enum class RequestMode
{
  CONFD = (1u << 0),
  XML = (1u << 1),
  PBRAW = (1u << 2)
};

}
#endif
