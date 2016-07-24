# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 2014/04/30
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwvcs)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

add_definitions(-Werror)

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
  trebuchet
  rwtasklet)

rift_python_install_tree(
  FILES
    rift/tasklets/__init__.py
    rift/tasklets/dts.py
    rift/tasklets/tasklet.py
    rift/tasklets/monkey.py
    rift/tasklets/tornado.py
  PYTHON3_ONLY
  COMPONENT rwvcs-1.0)

