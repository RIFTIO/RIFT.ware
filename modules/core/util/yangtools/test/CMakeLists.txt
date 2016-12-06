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
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs yang proto)
rift_add_subdirs(${subdirs})

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})

set(plugindir "${CMAKE_CURRENT_BINARY_DIR}/../plugins")
add_definitions(-DPLUGINDIR="${plugindir}")
add_definitions(-DINSTALLDIR="${CMAKE_INSTALL_PREFIX}")

if (NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  add_definitions(-DCONFD_ENABLED)

  include_directories(BEFORE ${CMAKE_BINARY_DIR}/confd/confd-build/libconfd/include)

endif ()

#
# Converted unit test target yangtools_gtest (runs with GTest and Celero)
##
find_library(boost_system_path NAMES boost_system)
if (NOT boost_system_path)
  message (FATAL_ERROR "Could not find library ${boost_system_path}")
endif ()

find_library(boost_filesystem_path NAMES boost_filesystem)
if (NOT boost_filesystem_path)
  message (FATAL_ERROR "Could not find library ${boost_filesystem_path}")
endif ()

find_library(boost_regex_path NAMES boost_regex)
if (NOT boost_regex_path)
  message (FATAL_ERROR "Could not find library ${boost_regex_path}")
endif ()

set(testdeps
  CF-${CF_VERSION}
  yangtools_yang
  yangtools_test_yang_gtest
  yangmodel_plugin-1.0
)

set(testlibs
  rwyang
  rwlib
  rwtrace
  rw_vx_plugin
  dl
  yangmodel_plugin-1.0
  rwplugin-1.0
  rwext_yang_gen
  yangtools_gtest_lib
  yangtools_test_proto
  ietf_yang_gen
  yangtools_test_yang_gtest_gen
  yangtools_test_yang_gi_gen

  # RIFT-5092 - this should be using rift_pkg_check_modules
  CoreFoundation

  ${boost_system_path}
  ${boost_filesystem_path}
  ${boost_regex_path}
)

set(GTEST_LIB_C_SOURCES
  ctype_lib.cpp       # C-Types testing
)

add_library(yangtools_gtest_lib SHARED ${GTEST_LIB_C_SOURCES})
target_link_libraries(yangtools_gtest_lib PRIVATE
  rwlib
  rwtrace
  rw_schema_pb
  protobuf-c
)

set(GTEST_C_SOURCES
  ctype_test.cpp            # C-Types testing
  ks_rrmerge_test.cpp       # keyspec reroot and merge tests
  leafref_test.cpp          # leafref testing
  pb_merge_test.cpp         # protobuf-c merge func unit test
  pbc_test.cpp              # protobuf-c unit test
  rw_keyspec_mk_test.cpp    # rw minikey unit tests.
  rw_keyspec_test.cpp       # rw_schema APIs unit tests
  rwxml_test.cpp            # RW XML documents unit tests
  test_appdata.cpp          # AppDataCache unit tests
  test_namespace.cpp        # rw_yang::Namespace tests
  test_pb_to_cli.cpp        # testing protobuf to cli
  test_yangmodel_plugin.cc  # Yangmodel vala plugin
  testncx.cpp               # libncx-based YangModel unit tests
  yangdom_test.cpp          # Yang-based RW XML document unit tests
  yangpbc_test.cpp          # yangpbc code generation tests
  pb_diff_test.cpp          # Protobuf Diff API test
  pbc_test_gencode.cpp      # protobuf-c unit tests converted to gtests
  pbc_test_gencode2.cpp     # protobuf-c unit tests converted to gtests
  pbc_delta_test.cpp        # Protobuf Delta API test
  test_pb_schema.cpp        # Dynamic PB Schema Merge tests and other tests
  yangjson_test.cpp         # Yang node to json conversion tests
  yangtest_common.cpp       # common test support functions
  test_rw_var_root.cpp      # testcases for rift var root
)

if(NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  list(APPEND GTEST_C_SOURCES confd_test.cpp iter_test.cpp)
  list(APPEND testlibs rwconfd ${CMAKE_BINARY_DIR}/confd/confd-build/libconfd/src/libconfd.so)
endif()

rift_gtest(yangtools_gtest
  ADD_SETUP
  TEST_SRCS ${GTEST_C_SOURCES}
  TEST_LIBS ${testlibs}
  DEPENDS ${testdeps}
)

rift_gtest(yangtools_gtest_long
  LONG_UNITTEST_TARGET
  TEST_SRCS pbc_perf_test.cpp ks_perf_test.cpp
  TEST_LIBS ${testlibs}
  DEPENDS ${testdeps}
)

# Add a gdb target for "handy" debugging
add_custom_target(yangtools_gdb
  COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      "GDB=1"
      "VERBOSE=1"
      ${CMAKE_CURRENT_BINARY_DIR}/yangtools_gtest
  DEPENDS yangtools_gtest)

##
#target_include_directories(testycli_gtest BEFORE PRIVATE
#  /usr/include/libxml2/libxml
#  /usr/include/libxml2
#  /usr/include/yuma/ncx
#  /usr/include/yuma/platform
#)
#
#add_custom_target(testycli_gdb
#  COMMAND
#    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
#      "GDB=1"
#      "VERBOSE=1"
#      ${CMAKE_CURRENT_BINARY_DIR}/testycli_gtest
#  DEPENDS testycli_gtest
#)
#
#add_custom_target(testycli_gdbcli
#  COMMAND
#    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
#      "GDB=1"
#      "VERBOSE=1"
#       ${CMAKE_CURRENT_BINARY_DIR}/testycli_gtest --cli
#  DEPENDS testycli_gtest
#)

##
# Add the python unittests
##
rift_py3test(yangmodel_plugin_pytest
  TEST_ARGS test_yangmodel_plugin.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangmodel_plugin-1.0
  )

rift_py3test(yangmodel_gi_pytest
  TEST_ARGS test_yangmodel_gi.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS RwYang_gi
  )

rift_py3test(rw_ypbc_pytest
  TEST_ARGS rw_ypbc_pytest.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangtools_test_yang_gi
  )

rift_py3test(rw_keyspec_pytest
  TEST_ARGS rw_ks_pytest.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangtools_test_yang_gi
  )

rift_py3test(yangmodel_gi_to_pb_cli_test
  TEST_ARGS test_pb_cli_gi.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS RwYang_gi
  )

rift_py3test(yangmodel_gi_yang_ext_test
  TEST_ARGS test_yang_ext_gi.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS RwYang_gi
  )

rift_py3test(rw_pbcm_test
  TEST_ARGS rw_pbcm_test.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangtools_test_yang_gi
  )

rift_py3test(rw_yang_json_test
  TEST_ARGS rw_test_yang_json.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangtools_test_yang_gi
  )

rift_py3test(rw_test_gi_pb_yaml_json
  TEST_ARGS test_gi_pb_yaml_json.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangtools_test_yang_gi
  )

rift_py3test(test_gi_pbc
  LONG_UNITTEST_TARGET
  TEST_ARGS test_gi_pbc.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangtools_test_yang_gi
  )

rift_py3test(rw_gi_thread_test
  LONG_UNITTEST_TARGET
  TEST_ARGS rw_gi_thread_test.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangtools_test_yang_gi
  )

rift_pytest(testoutput_pytest
  TEST_ARGS test-rpc-output.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS yangtools_test_yang_gi
  )

