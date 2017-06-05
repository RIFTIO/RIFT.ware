# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 2014/03/20
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Add the target to run the rwmain executable
##
rift_unittest(run_rwcli
  EXCLUDE_FROM_UNITTEST_TARGETS
  TEST_ARGS
    ${PROJECT_TOP_DIR}/bin/envset.sh
    gdb --args ${CMAKE_INSTALL_PREFIX}/usr/bin/rwmain
    load manifest file ${CMAKE_CURRENT_SOURCE_DIR}/rw-manifest.xml
  TEST_WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}
  DEPENDS rwmain
)

##
# Allow specific compiler warnings
##
rift_allow_compiler_warning(unused-but-set-variable)
rift_allow_compiler_warning(unused-parameter)

set(plugindir "${CMAKE_CURRENT_BINARY_DIR}/../plugins")
add_definitions(-DPLUGINDIR="${plugindir}")
add_definitions(-DINSTALLDIR="${CMAKE_INSTALL_PREFIX}")

##
# Set some RIFT specific VALA variables for this cmake project
##
set(TEST_PROGRAM.TARGET test_rwcli_plugin)

## 
# Adds an executable target called <name> to be built from the source files 
# listed in the command invocation
# The <name> corresponds to the logical target name and must be globally 
# unique within a project
##
add_executable(${TEST_PROGRAM.TARGET} test_rwcli_plugin.c)

##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a 
# command such as add_executable or add_library
# This basic plugin depends on LIBPEAS and GI libraries
##
target_link_libraries(${TEST_PROGRAM.TARGET}
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwcli/vala/librwcli_plugin-1.0.so
  ${RIFT_GLOBAL_LIBPEAS_LIBRARIES}
  ${CMAKE_INSTALL_PREFIX}/usr/lib/librwlib.so
  ${CMAKE_INSTALL_PREFIX}/usr/lib/librw_vx_plugin.so
  ${CMAKE_INSTALL_PREFIX}/usr/lib/rift/plugins/librwplugin-1.0.so
)
add_dependencies(${TEST_PROGRAM.TARGET} rwcli_plugin-1.0)

##
# The following code adds targets for generating the rwut executables
##
include_directories(BEFORE /usr/include/libxml2)

# Whole bunch of libraries can this be simplified?
set(rwcli_utest_libs
  rwcli
  rwcli_plugin-1.0
  rwuagentproto
  CoreFoundation
  rwmsg
  nanomsg
  xerces-c
  protobuf-c
  glib-2.0 
  peas-1.0
  stdc++
  rwlib
  rwtrace
  rwsched
  rwlog
  rwyang
  talloc
  rw_tasklet_plugin-1.0
  rw_vx_plugin
  rwplugin-1.0
  ncx 

  # System libraries
  xml2
)

rift_gtest(yangcli_test
  TEST_SRCS yangcli_test.cpp
  TEST_LIBS ${rwcli_utest_libs}
  DEPENDS rwcli rwcli-c
)


rift_gtest(rwcli_utest
  TEST_SRCS rwcli_test.cpp
  TEST_LIBS ${rwcli_utest_libs}
  DEPENDS rwcli rwcli-c rwcli_plugin-1.0
)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

rift_gtest(rwcli_schema_test
  TEST_SRCS rwcli_schema_test.cpp
  TEST_LIBS ${rwcli_utest_libs}
  DEPENDS rwcli rwcli-c
)

rift_unittest(rwcli_writer_test
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwcli_writer_test.py
)

