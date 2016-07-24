# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# 

cmake_minimum_required(VERSION 2.8)

##
# Generate the protobuf file -- This will GO AWAY once we can do this via yang
# TODO: CLEAN THIS UP
##
set(proto_files rwuagent.proto)
rift_add_proto_target(
  TARGET rwuagent_proto_parse
  WITH_DSO
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  COMPONENT ${PKG_LONG_NAME}
)

##
# Add the target for the rwuagent library
##
add_library(rwuagentproto SHARED ${proto_c_files})
target_link_libraries(rwuagentproto PRIVATE
  rwmsg
  protobuf-c)
install(TARGETS rwuagentproto LIBRARY DESTINATION usr/lib COMPONENT ${PKG_LONG_NAME})

