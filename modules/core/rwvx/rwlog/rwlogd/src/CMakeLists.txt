# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
  ${CMAKE_INSTALL_PREFIX}/usr/lib/libdispatch.so
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
install(TARGETS rwlogd_api rwlogd_show LIBRARY DESTINATION usr/lib COMPONENT ${PKG_LONG_NAME})
