# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

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
install(FILES ../include/riftware/rwuagent_msg_client.h DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})
install(TARGETS rwuagent_msg_client DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

