# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwuagent)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs client src plugins test proxy-example)

if(NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  list(APPEND subdirs confd)
endif()

rift_add_subdirs(SUBDIR_LIST ${subdirs})
