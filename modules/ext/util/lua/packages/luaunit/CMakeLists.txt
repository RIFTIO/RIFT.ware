# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#                                                              
# Author(s): Anil Gunturu
# Creation Date: 01/08/2015
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME luaunit)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

install(FILES luaunit/luaunit.lua
  DESTINATION usr/share/lua/5.1 COMPONENT ${PKG_LONG_NAME})



