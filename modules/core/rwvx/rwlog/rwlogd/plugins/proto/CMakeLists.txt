# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 2014/05/12
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

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
  COMPONENT ${INSTALL_COMPONENT}
  )

##
# Add the target for the library
##
add_library(rwlogdproto SHARED ${proto_c_files})
target_link_libraries(rwlogdproto PRIVATE
  rwmsg
  protobuf-c)
install(TARGETS rwlogdproto LIBRARY DESTINATION usr/lib 
    COMPONENT ${INSTALL_COMPONENT})
