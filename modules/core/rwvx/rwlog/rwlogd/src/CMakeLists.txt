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
# Author(s): Sameer Nayak
# Creation Date: 11/30/2014
# 
cmake_minimum_required(VERSION 2.8)


##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/rwlogd/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include)

add_library(rwlogd_show SHARED
  rwlog_display.cpp
)

target_link_libraries(rwlogd_show
  rwschema_yang_gen
  rwlog-mgmt_yang_gen
  rwdts_api
  rwlib)

#add_definitions(-std=gnu99 -D_GNU_SOURCE)

add_library(rwlogd_api SHARED
  rwlogd_rx.c
  rwlogd_plugin_mgr.c
  rwlog_mgmt.c
  rwlogd_srv_api_handlers.c
  )
target_link_libraries(rwlogd_api
  rwschema_yang_gen
  rwlog-mgmt_yang_gen
  rwlogd_show
  dispatch
  rwlogd_sink
  rwvcs
  rwdynschema
  rwlib
  talloc
  rwpython_util-1.0
  rw_vx_plugin
  rwsched
  rwtrace
  nanomsg
  glib-2.0
  protobuf-c
  CoreFoundation
  rwdts_api
  rt
  pthread
  estw
  pcap

  rwshell_api)

  #rw_vcs_yang_gen

add_dependencies(rwlogd_api rwlogd_show rwlogd_sink rwdynschema)
add_dependencies(rwlogd_api estw)
install(TARGETS rwlogd_api rwlogd_show LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
