# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 8/29/2013
# 

set(VALA_NAME feature_plugin)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})

rift_pkg_check_modules(PEAS REQUIRED libpeas-1.0>=1.12.2)
include_directories(BEFORE
  ${PEAS_INCLUDE_DIRS})

##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a 
# command such as add_executable or add_library
##
set(feature_plugin_libs
  math_common-1.0
  ${VALA_LONG_NAME}
  ${PEAS_LIBRARIES})

add_definitions(
  -DINSTALLDIR="${CMAKE_INSTALL_PREFIX}"
  -DPLUGINDIR="${CMAKE_CURRENT_BINARY_DIR}/../vala"
  )

##
# Add the basic plugin google test
##
rift_gtest(feature_plugin_gtest
  TEST_SRCS feature_plugin_gtest.cc
  TEST_LIBS ${feature_plugin_libs}
  DEPENDS ${VALA_NAME}-c ${VALA_LONG_NAME} ${VALA_NAME}-py
  )
