# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 2014/05/22
# 

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})

set(PKG_NAME rwdtsmockserver)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs
  plugins
  )

# Install the crossbar server configuration
install(
  DIRECTORY etc/crossbar
  DESTINATION etc
  )

rift_add_subdirs(SUBDIR_LIST ${subdirs})
