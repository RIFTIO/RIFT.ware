# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 5/12/2014
# 

set(C_SOURCES libev_perftest.cpp)
add_executable(libev_perftest ${C_SOURCES})
target_link_libraries (libev_perftest
  ev
  ${CMAKE_INSTALL_PREFIX}/usr/lib/rwut/libgmock.a
  ${CMAKE_INSTALL_PREFIX}/usr/lib/rwut/libgmock_main.a
  pthread
  rwsched
  rwlog
  dispatch
  BlocksRuntime
  glib-2.0
  rwlib
  CoreFoundation
  talloc)

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
