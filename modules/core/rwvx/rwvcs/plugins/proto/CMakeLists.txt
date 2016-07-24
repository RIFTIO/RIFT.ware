# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 2014/05/12
# 

cmake_minimum_required(VERSION 2.8)

##
# Generate the protobuf file -- This will GO AWAY once we can do this via yang
# TODO: CLEAN THIS UP
##

set(proto_files rwtoytasklet.proto)
rift_add_proto_target(
  TARGET rwtoytasklet_proto_parse
  WITH_DSO
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  COMPONENT ${PKG_LONG_NAME}
)

##
# Add the target for the library
##
add_library(rwtoytaskletproto SHARED ${proto_c_files})
target_link_libraries(rwtoytaskletproto PRIVATE
  rwmsg
  protobuf-c)
install(TARGETS rwtoytaskletproto LIBRARY DESTINATION usr/lib
  COMPONENT ${PKG_LONG_NAME})

