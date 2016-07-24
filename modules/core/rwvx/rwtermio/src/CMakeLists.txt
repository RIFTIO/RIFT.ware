# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Balaji Rajappa
# Creation Date: 8/25/2015
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwtermio)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/rwlogd/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include
)

##
# Add the target for the rwtermiotasklet library
##
set(C_SOURCES rwtermio.c)
add_library(rwtermio STATIC ${C_SOURCES})

add_dependencies(rwtermio rw_tasklet_plugin-1.0 rwsched)

