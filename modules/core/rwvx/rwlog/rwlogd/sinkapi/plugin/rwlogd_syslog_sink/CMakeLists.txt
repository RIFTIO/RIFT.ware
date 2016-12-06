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
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 3/17/2014
# 
cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../../cmake/modules")

#add_definitions(-std=gnu99)

set(PKG_NAME rwlogd_syslog_sink)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/rwlogd/sinkapi/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/rwlogd/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src)


add_library(rwlogd_syslog_sink SHARED
  rwlogd_syslog_sink.cpp
  )
target_link_libraries(rwlogd_syslog_sink
  rwdts_api
  rwschema_yang_gen
  rwlib
  rwmsg
  rwpython_util-1.0
  rw_vx_plugin
  rwtrace
  rwyang
  nanomsg
  glib-2.0
  protobuf-c
  rwlogd_sink
  CoreFoundation)

add_dependencies(rwlogd_syslog_sink rwlog rwvcs rwschema_yang.headers)

install(TARGETS rwlogd_syslog_sink LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
