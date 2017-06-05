# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 2014/05/22
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})

set(PKG_NAME rwdtsperf)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdtsperf/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwschema/src)

#set(subdirs plugins test)
set(subdirs 
  src
  plugins
  test)

rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW.DTSPerf API Reference"
  DEST_DIR ${PKG_NAME})

