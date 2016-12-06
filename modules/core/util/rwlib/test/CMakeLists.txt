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
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

rift_allow_compiler_warning(ignored-qualifiers)

#
# Add the preprocessor definitions
##
remove_definitions(-DPLUGINDIR="${plugindir}")
set(plugindir "${RIFT_SUBMODULE_BINARY_ROOT}/rwlib/plugins/python_plugin/vala")

add_definitions(
  -DPLUGINDIR="${plugindir}"
  -DPYDIR="${CMAKE_CURRENT_BINARY_DIR}"
  -DINSTALLDIR="${CMAKE_INSTALL_PREFIX}"
  )

##
# Converted unit test target (with GTest and Celero)
##

rift_pkg_check_modules(GOBJECT_IS REQUIRED gobject-introspection-1.0>=1.42.0)
rift_pkg_check_modules(PEAS REQUIRED libpeas-1.0>=1.12.2)
rift_pkg_check_modules(TALLOC REQUIRED talloc>=2.1.0)

include_directories(BEFORE
  ${GOBJECT_IS_INCLUDE_DIRS}
  ${PEAS_INCLUDE_DIRS}
  ${TALLOCINCLUDE_DIRS})

set (rwlib_utest_lib
  rwlib
  unwind unwind-x86_64 unwind-ptrace unwind-coredump
  python_plugin-1.0
  rw_vx_plugin
  rwplugin-1.0
  standard_plugin-1.0
  feature_plugin-1.0
  ${GOBJECT_IS_LIBRARIES}
  ${PEAS_LIBRARIES}
  ${TALLOC_LIBRARIES}
)

rift_gtest(rwlib_utest
  ADD_SETUP 
  TEST_SRCS rwlib_utest.cc rw_resource_track_utest.cc rw_fsm_utest.cpp
  TEST_LIBS ${rwlib_utest_lib}
  DEPENDS feature_plugin-1.0 standard_plugin-1.0
  )

##
# Copy the rwlib_test_static_assert.py and substitute the C_INCLUDES flags
# This is used for testing static asserts
##
foreach(dir ${PEAS_INCLUDE_DIRS} ${GOBJECT_INCLUDE_DIRS} ${TALLOC_INCLUDE_DIRS})
  set(test_c_flags "${test_c_flags} -I${dir}")
endforeach(dir)

foreach(dir rwlib rwtypes)
  set(test_c_flags "${test_c_flags} -I${RIFT_SUBMODULE_SOURCE_ROOT}/${dir}/include/riftware")
endforeach()
set(test_c_flags "${test_c_flags}")


add_custom_target(rwlib_copy_test_static_assert_script ALL
  ${CMAKE_COMMAND} -E
  copy ${CMAKE_CURRENT_SOURCE_DIR}/rwlib_test_static_assert.py .
  COMMAND
  sed -i -e 's%C_INCLUDES%${test_c_flags}%g' rwlib_test_static_assert.py
  COMMAND
  sed -i -e 's%LD_FLAGS%${test_ld_flags}%g' rwlib_test_static_assert.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

add_executable(rw_resource_annotate_test rw_resource_annotate_test.c)
##
# Set the list of libraries to link with
##
target_link_libraries(rw_resource_annotate_test PRIVATE
  rwlib
  pthread)

rift_pytest(test_rwlib_gi
  TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test_rwlib_gi.py)
