# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
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
  protobuf-c
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

