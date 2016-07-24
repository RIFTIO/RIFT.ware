# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Sanil Puthiyandyil
# Creation Date: 02/06/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

## 
# This should be added as an extrenal project - TBD
# This file serves as a basic template for building
# static and shared libraries
##

##
# Specify the target name for the library
##
set(LUA_PACKAGES_TARGET struct)
set(PKG_NAME lua_packages)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Sepcify the list of source files
##

set(C_SOURCES 
  struct.c 
  )

##
# This creates a libarary
##
set(CMAKE_SHARED_LIBRARY_PREFIX "")
add_library(${LUA_PACKAGES_TARGET} SHARED ${C_SOURCES})

##
# Specificy the targets that the library target depends on
# This target doesn't have any dependencies so the following
# line is commented out
##

install(TARGETS ${LUA_PACKAGES_TARGET} LIBRARY DESTINATION usr/lib/lua/5.1  COMPONENT ${PKG_LONG_NAME})
