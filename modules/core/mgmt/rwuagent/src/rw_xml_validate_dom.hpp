
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
 * @file rw_xml_dom_merger.hpp
 * @brief Helper to validate config DOM
 */


#ifndef RW_XML_VALIDATE_HPP_
#define RW_XML_VALIDATE_HPP_


#include <forward_list>
#include <string>
#include <memory>

#include "rw_app_data.hpp"
#include "rw_xml.h"
#include "rw-mgmtagt-log.pb-c.h"

#define RW_VAL_DOM_Paste3(a,b,c) a##_##b##_##c

#define RW_VALIDATE_DOM_INST_LOG_Step1(__instance__, evvtt, ...)                  \
 RWLOG_EVENT(__instance__, evvtt, (uint64_t)__instance__, __VA_ARGS__)

#define RW_LOG(__instance__, evvtt, ...) \
 if (__instance__) { \
  RW_VALIDATE_DOM_INST_LOG_Step1(__instance__, RW_VAL_DOM_Paste3(RwMgmtagtLog,notif,evvtt), __VA_ARGS__) \
 }

namespace rw_uagent {
  class Instance;
}

namespace rw_yang {


class ValidationStatus {
 private:
  rw_status_t const status_;
  std::string const reason_;

  ValidationStatus() = delete;

 public:
  ValidationStatus(rw_status_t const status);
  ValidationStatus(rw_status_t const status, std::string const & reason);  
  ValidationStatus(ValidationStatus const & other);
  ValidationStatus(ValidationStatus const && other);

  rw_status_t status();
  bool failed();
  std::string reason();
};

ValidationStatus validate_dom(XMLDocument* dom, rw_uagent::Instance* inst = nullptr);

}

#endif
