# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

set(VALA_NAME test_nonglobal_loader)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})

rift_pkg_check_modules(PEAS REQUIRED libpeas-1.0>=1.12.2)

##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a 
# command such as add_executable or add_library
##
set(nonglobal_loader_libs
  ${VALA_LONG_NAME}
  ${PEAS_LIBRARIES})

add_definitions(
  -DINSTALLDIR="${CMAKE_INSTALL_PREFIX}"
  -DPLUGINDIR="${CMAKE_CURRENT_BINARY_DIR}/../vala"
  )

##
# Add the basic plugin google test
##
#rift_gtest(nonglobal_loader_gtest
#  TEST_SRCS nonglobal_loader_gtest.cc
#  TEST_LIBS ${nonglobal_loader_libs} rw_vx_plugin rwlib
#  DEPENDS ${VALA_LONG_NAME} ${VALA_NAME}-lua rw_vx_plugin
#  )
