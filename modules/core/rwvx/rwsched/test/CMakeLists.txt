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
# Author(s): Austin Cormier
# Creation Date: 5/12/2014
# 

set(C_SOURCES libev_perftest.cpp)
add_executable(libev_perftest ${C_SOURCES})
target_link_libraries (libev_perftest
  ev
  gmock
  gmock_main
  pthread
  rwsched
  rwlog
  dispatch
  BlocksRuntime
  glib-2.0
  rwlib
  CoreFoundation
  talloc)

# FC20 has some files in /usr/include/libev; other distros have them in /usr/include
include_directories(
  /usr/include/libev
  )

rift_gtest(rwsched_unittest
  TEST_SRCS 
    rwsched_unittest.cpp toysched_utest.cpp rwsched_dispatch_tests.cpp 
    rwsched_cfrunloop_tests.cpp
  TEST_LIBS
    rwsched
    rwlog
    dispatch
    BlocksRuntime
    glib-2.0
    rwlib
    talloc
    CoreFoundation
    rw_vx_plugin
    rwplugin-1.0
    rwtypes
    async_cb-1.0

  DEPENDS rwsched rwlog async_cb-c async_cn-python
)

##
# Converted unit test target that combines the old toysched_unittest.cpp
# and toysched_perftest.cpp (runs with GTest and Celero)
##
rift_unittest_build(fd_runloop_hangs
  TEST_SRCS fd_runloop_hangs.cpp
  TEST_LIBS
    BlocksRuntime
    CoreFoundation
    dispatch
    glib-2.0
    talloc

    rwlib
    rwlog
    rwsched
)

rift_py3test(test_rwsched_gi
  TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test_rwsched_gi.py)
