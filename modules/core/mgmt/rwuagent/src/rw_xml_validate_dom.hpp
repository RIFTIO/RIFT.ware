
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
