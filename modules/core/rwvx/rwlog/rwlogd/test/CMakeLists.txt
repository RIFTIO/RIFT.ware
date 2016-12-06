# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs yang)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

install(FILES packet.pkt DESTINATION usr/bin/ COMPONENT ${INSTALL_COMPONENT})
install(FILES inbound_gtp.pkt DESTINATION usr/bin/ COMPONENT ${INSTALL_COMPONENT})
install(FILES outbound_gtp.pkt DESTINATION usr/bin/ COMPONENT ${INSTALL_COMPONENT})

include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_ROOT}/module/core/mgmt/rwcli/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/rwlogd/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/rwlogd/sinkapi/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include)

# RIFT-5033
rift_gtest(rwlogd_gtest
  TEST_SRCS rwlogd_gtest.cc
  TEST_LIBS
    rwlog
    log_test_yang_gen
    dynschema_log_test_yang_gen
    rwlib
    rwlogd-c
    rwlogdproto
    uuid
    dl
  DEPENDS rwlogd_api
)

