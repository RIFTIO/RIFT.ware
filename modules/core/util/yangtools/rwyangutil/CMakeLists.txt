# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Arun Muralidharan
# Creation Date: 2015/10/01
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_pkg_config)
# rw_file_proto_ops is dependednt on some 
# rift libraries. It is meant to be run in conjunction
# with rift-ware.

cmake_minimum_required(VERSION 2.8)

add_definitions(-std=c++11)

if (NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  add_definitions(-DCONFD_ENABLED)
endif ()

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(boost_filesystem_path NAMES boost_filesystem)
if (NOT boost_filesystem_path)
  message (FATAL_ERROR "Could not find library ${boost_filesystem_path}")
endif ()
find_library(boost_system_path NAMES boost_system)
if (NOT boost_system_path)
  message (FATAL_ERROR "Could not find library ${boost_system_path}")
endif ()

set(
  CXX_SOURCES
    rwyangutil_argument_parser.cc
    rwyangutil_directory_helpers.cc
    rwyangutil_file_proto_ops.cc
    rwyangutil_helpers.cc
  )

add_library(
  rwyangutil_lib
  SHARED
    ${CXX_SOURCES}
  )

install(
  TARGETS
    rwyangutil_lib
  LIBRARY
  DESTINATION
    usr/lib
  COMPONENT
    ${INSTALL_COMPONENT}
  )

add_executable(
  rwyangutil
  rwyangutil.cc
  )

target_link_libraries(rwyangutil
  rwyangutil_lib
  ${boost_filesystem_path}
  ${boost_system_path}
)

target_link_libraries(rwyangutil_lib
  ${boost_filesystem_path}
  ${boost_system_path}
)

install(TARGETS rwyangutil
 DESTINATION usr/bin
 COMPONENT ${INSTALL_COMPONENT})

install(
  FILES
    rwyangutil.h
    rwyangutil_argument_parser.hpp
    rwyangutil_file_proto_ops.hpp
    rwyangutil_helpers.hpp
  DESTINATION
    usr/include
  COMPONENT
    ${INSTALL_COMPONENT}
  )

add_subdirectory(test)
