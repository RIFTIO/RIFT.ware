# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 3/17/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
cmake_minimum_required(VERSION 2.8)

##
# Set the path to the top level cmake modules directory
##
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../cmake/modules")

add_definitions( -fPIC)

set(PKG_NAME rwlogd_sink)
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
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/rwlogd/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src)

add_library(rwlogd_sink SHARED
  rwlogd_sink_common.cpp
  rwlogd_filters.cpp
  rwlogd_default_sink.cpp
  rwlogd_file_sink.cpp
  rwlogd_console_sink.cpp
  rwlogd_consistent_hash.cpp
  )

target_link_libraries(rwlogd_sink
  rwlib
  rwpython_util-1.0
  rw_tasklet_plugin-1.0
  rw_vx_plugin
  rwtrace
  rwyang
  rwmsg
  rwlogdproto
  rwdynschema
  nanomsg
  glib-2.0
  protobuf-c
  CoreFoundation
  dl
  uuid
  rt
  tcpdump
  rwlog
  rwdts_api
  -lcrypto
  -lpcap
  -lpthread)

add_dependencies(rwlogd_sink rwlogdproto rwdynschema rwdts)

install(TARGETS rwlogd_sink LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
