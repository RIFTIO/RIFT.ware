# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwlog)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

rift_allow_compiler_warning(maybe-uninitialized)
#Force -Werror for fpath
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")


add_definitions(-D_GNU_SOURCE)

##
# Add the subdirectories
##
set(subdirs 
  plugins
  rwlogd
  src
  test)

rift_add_subdirs(SUBDIR_LIST ${subdirs})
