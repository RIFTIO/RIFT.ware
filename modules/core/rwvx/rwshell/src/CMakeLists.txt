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
# Author(s): Tim Mortsolf
# Creation Date: 05/22/2014
# 

cmake_minimum_required(VERSION 2.8)

add_definitions(-std=gnu99)

##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include)

add_library(rwshell_api SHARED
  rwshell_py.c
  rwshell_pt.c
  rwshell_ct.c

  rwshell_mgmt.c)

target_link_libraries(rwshell_api PRIVATE
  rwshell-1.0
  rwshell-mgmt_yang_gen
  rwlib
  talloc
  dispatch
  rwsched
  CoreFoundation
  rwvcs
  rwdts_api
  rw_vx_plugin
  peas-1.0)

add_dependencies(rwshell_api rwmanifest_yang.headers)

install(
  TARGETS rwshell_api 
  LIBRARY DESTINATION usr/lib 
  COMPONENT ${INSTALL_COMPONENT}
)

add_subdirectory(rwshell)
