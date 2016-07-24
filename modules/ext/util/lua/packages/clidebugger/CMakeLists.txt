# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)                                                              
# Author(s): Anil Gunturu
# Creation Date: 01/08/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME clidebugger)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

install(FILES clidebugger/debugger.lua
  DESTINATION usr/share/lua/5.1 COMPONENT ${PKG_LONG_NAME})



