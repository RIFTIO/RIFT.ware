# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

set(VALA_NAME lua_coroutine)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})

rift_pkg_check_modules(PEAS REQUIRED libpeas-1.0>=1.12.2)

##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a 
# command such as add_executable or add_library
##
set(lua_coroutine_libs
  ${VALA_LONG_NAME}
  ${PEAS_LIBRARIES})

add_definitions(
  -DINSTALLDIR="${CMAKE_INSTALL_PREFIX}"
  -DPLUGINDIR="${CMAKE_CURRENT_BINARY_DIR}/../vala"
  )

##
# Add the lua coroutine google test
##
rift_gtest(lua_coroutine_gtest
  TEST_SRCS lua_coroutine_gtest.cc
  TEST_LIBS ${lua_coroutine_libs}
  DEPENDS ${VALA_NAME}-c ${VALA_LONG_NAME} ${VALA_NAME}-py
  )

##
# This is a standalone c program. There seems to be issue with C++ capturing
# proper errors when Lua throws exceptions. This is quick c program to test
# Lua code, incase there is an error.
##
add_executable(lua_coroutine_test lua_coroutine_test.c)
target_link_libraries(lua_coroutine_test
  ${VALA_LONG_NAME}
  ${PEAS_LIBRARIES})
