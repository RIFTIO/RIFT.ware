# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 2015/09/11
# 

cmake_minimum_required(VERSION 2.8)

rift_pkg_check_modules(GOBJECT REQUIRED gobject-2.0>=2.42.0)
include_directories(BEFORE
  ${GOBJECT_INCLUDE_DIRS}
)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DINSTALLDIR="${CMAKE_INSTALL_PREFIX}")

set(rwmemlog_testlibs
  rwlib
  rwmemlog
  rwmemlog_yang_gen
  ${GOBJECT_LIBRARIES}
  ${CMAKE_INSTALL_PREFIX}/usr/lib/libprotobuf-c.so
)

set(rwmemlog_gtest_sources
  rwmemlog_test.cpp
  rwmemlog_c_test.c
)

rift_gtest(rwmemlog_gtest
  TEST_SRCS ${rwmemlog_gtest_sources}
  TEST_LIBS ${rwmemlog_testlibs}
)

set(testdeps
  rwmemlog_gtest
)

# Add a gdb target for "handy" debugging
add_custom_target(rwmemlog_gdb
  COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      "GDB=1"
      "VERBOSE=1"
      ${CMAKE_CURRENT_BINARY_DIR}/rwmemlog_gtest
  DEPENDS rwmemlog_gtest
)

