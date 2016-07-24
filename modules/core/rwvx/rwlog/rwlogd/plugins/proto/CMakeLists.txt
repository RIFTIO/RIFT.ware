# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 2014/05/12
# 

cmake_minimum_required(VERSION 2.8)

##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

##
# Generate the protobuf file -- This will GO AWAY once we can do this via yang
# TODO: CLEAN THIS UP
##

set(proto_files rwlogd.proto)

rift_add_proto_target(
  TARGET rwlogd_proto_parse
  WITH_DSO
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  COMPONENT ${PKG_LONG_NAME}
  )

##
# Add the target for the library
##
add_library(rwlogdproto SHARED ${proto_c_files})
target_link_libraries(rwlogdproto PRIVATE
  rwmsg
  protobuf-c)
install(TARGETS rwlogdproto LIBRARY DESTINATION usr/lib 
    COMPONENT ${PKG_LONG_NAME})
