# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 05/22/2014
# 

cmake_minimum_required(VERSION 2.8)

add_definitions(-std=gnu99)

add_library(rwcal_api SHARED
  rwcal_py.c
  rwcal_rwzk.c)

target_link_libraries(rwcal_api PRIVATE
  rwcal-1.0
  rwcal_yang_gen
  rwlib
  rw_vx_plugin
  peas-1.0)

add_dependencies(rwcal_api rwmanifest_yang.headers)

install(TARGETS rwcal_api LIBRARY DESTINATION usr/lib COMPONENT ${PKG_LONG_NAME})

install(PROGRAMS rwvim.py DESTINATION usr/bin COMPONENT ${PKG_LONG_NAME})
