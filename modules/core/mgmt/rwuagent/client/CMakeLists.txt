# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwuagentclient)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

add_library(rwuagent_msg_client SHARED rwuagent_msg_client.c)
target_link_libraries(rwuagent_msg_client PRIVATE
  rwlib
  rwmsg
  rwuagentproto
  rwtasklet
  rwvcs

  CoreFoundation
  glib-2.0
  protobuf-c)
install(FILES ../include/riftware/rwuagent_msg_client.h DESTINATION usr/include COMPONENT ${PKG_LONG_NAME})
install(TARGETS rwuagent_msg_client DESTINATION usr/lib COMPONENT ${PKG_LONG_NAME})

