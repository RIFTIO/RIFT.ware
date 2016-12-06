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
# Creation Date: 2014/04/30
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwvcs)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwschema/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src)

rift_add_subdirs(SUBDIR_LIST
  plugins/yang
  plugins/vala
  plugins/rwtoytasklet-c
  plugins/pytoytasklet
  plugins/proto
  ra
  reaper
  rwmain
  src
  test
  rwtasklet)

rift_python_install_tree(
  FILES
    rift/tasklets/__init__.py
    rift/tasklets/dts.py
    rift/tasklets/tasklet.py
    rift/tasklets/monkey.py
    rift/tasklets/tornado.py
    rift/tasklets/registry.py
  PYTHON3_ONLY
  COMPONENT ${INSTALL_COMPONENT})


rift_python_install_tree(
  FILES
    rift/rwzk_intf/__init__.py
    rift/rwzk_intf/rwzk.py
  PYTHON3_ONLY
  COMPONENT ${INSTALL_COMPONENT})
