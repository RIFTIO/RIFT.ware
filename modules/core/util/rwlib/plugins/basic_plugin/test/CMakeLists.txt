# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

set(VALA_NAME basic_plugin)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})

rift_pkg_check_modules(PEAS REQUIRED libpeas-1.0>=1.12.2)

##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a 
# command such as add_executable or add_library
##
set(basic_plugin_libs
  ${VALA_LONG_NAME}
  ${PEAS_LIBRARIES})

add_definitions(
  -DINSTALLDIR="${CMAKE_INSTALL_PREFIX}"
  -DPLUGINDIR="${CMAKE_CURRENT_BINARY_DIR}/../vala"
  )

##
# Add the basic plugin google test
##
rift_gtest(basic_plugin_gtest
  TEST_SRCS basic_plugin_gtest.cc
  TEST_LIBS ${basic_plugin_libs}
  DEPENDS ${VALA_NAME}-c ${VALA_LONG_NAME} ${VALA_NAME}-py basic_plugin-1.0
  )

##
# Add the basic plugin python test
##

rift_unittest(basic_plugin_pytest
  TEST_ARGS python3 basic_plugin_pytest.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
