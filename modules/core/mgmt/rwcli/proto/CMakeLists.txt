# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 3/25/2014
# 

cmake_minimum_required(VERSION 2.8)

##
# Add flags to the compiler command line for sources in the current directory
##
add_definitions(-DINSTALLDIR="${CMAKE_INSTALL_PREFIX}")

##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

##
# Generate the protobuf file -- This will GO AWAY once we can do this via yang
# TODO: CLEAN THIS UP
##
set(proto_files rwcli.proto)
rift_add_proto_target(
  WITH_DSO
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  COMPONENT ${PKG_LONG_NAME}
)

##
# Add the target for the rwcli library
##
add_library(rwcliproto STATIC ${proto_c_files})
install(TARGETS rwcliproto ARCHIVE DESTINATION usr/lib COMPONENT ${PKG_LONG_NAME})

