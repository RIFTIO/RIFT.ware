# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

# Add a gdb target for "handy" debugging
add_custom_target(uagent_utest_gdb
  COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      "GDB=1"
      "VERBOSE=1"
      ${CMAKE_CURRENT_BINARY_DIR}/rwuagent_utest
  DEPENDS rwuagent_utest
)

add_custom_target(uagent_utest_valgrind
  COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      "VALGRIND=1"
      "VERBOSE=1"
      ${CMAKE_CURRENT_BINARY_DIR}/rwuagent_utest
  DEPENDS rwuagent_utest
)

##
# Set the list of directories to search for header files
##
include_directories(BEFORE
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwuagent/test
  ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(
  ${CMAKE_INSTALL_PREFIX}/usr/include/rift/plugins/
  ${RIFT_GI_INCLUDE_DIRS}
  ${RIFT_LIBUNWIND_INCLUDE_DIRS}
  ${RIFT_LIBPEAS_INCLUDE_DIRS})

find_library(boost_filesystem_path NAMES boost_filesystem)
if (NOT boost_filesystem_path)
  message (FATAL_ERROR "Could not find library ${boost_filesystem_path}")
endif ()
find_library(boost_system_path NAMES boost_system)
if (NOT boost_system_path)
  message (FATAL_ERROR "Could not find library ${boost_system_path}")
endif ()

set(test_libs
  rwuagent
  reaper
  reaper_client
  uagent_test_yang_gen
  rwuagentproto
  rwvcs
  rwtasklet
  rwdts_api
  rwdts_router
  rwyang
  rwyangutil_lib
  rwdtsproto
  rwmsg
  rwyang
  rwmsgbroker
  rwlib
  rwtrace
  talloc
  nanomsg
  protobuf-c
  rwsched
  rwlog
  CoreFoundation
  dispatch
  BlocksRuntime
  ck
  glib-2.0
  ${boost_filesystem_path}
  ${boost_system_path}
)

if (NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  list(APPEND test_libs 
       rwconfd
       ${CMAKE_INSTALL_PREFIX}/usr/local/confd/lib/libconfd.so
      )
endif()

set(test_deps
  rwuagent
  uagent_test_yang.headers
)

rift_gtest(rwuagent_utest
  TEST_SRCS
    async_file_writer_test.cpp
    rwuagent_gtest.cc
    rw_xml_dom_merger_test.cpp
    rw_xml_validation_test.cpp
  TEST_LIBS ${test_libs}
  DEPENDS ${test_deps}
  TEST_ARGS --gtest_shuffle --gtest_repeat=1
)

##
# Yang targets
##
rift_add_yang_target(
  TARGET
    uagent_test_yang
  YANG_FILES
    vehicle.yang
    uagent-utest.yang
    validation.yang
    validation-other.yang
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

set(subdirs utframework)
rift_add_subdirs(${subdirs})
