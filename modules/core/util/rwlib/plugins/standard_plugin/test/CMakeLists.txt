# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

set(VALA_NAME standard_plugin)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})

rift_pkg_check_modules(PEAS REQUIRED libpeas-1.0>=1.12.2)
include_directories(BEFORE
  ${PEAS_INCLUDE_DIRS})

##
# Set some RIFT specific VALA variables for this cmake project
##
set(TEST_PROGRAM.TARGET test_${VALA_NAME})

## 
# Adds an executable target called <name> to be built from the source files 
# listed in the command invocation
# The <name> corresponds to the logical target name and must be globally 
# unique within a project
##
add_executable(${TEST_PROGRAM.TARGET} test_${VALA_NAME}.c)

##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a 
# command such as add_executable or add_library
# This standard plugin depends on LIBPEAS and GI libraries
##
target_link_libraries(${TEST_PROGRAM.TARGET} PRIVATE
  rwplugin-1.0
  ${VALA_LONG_NAME}
  ${PEAS_LIBRARIES})

add_dependencies(${TEST_PROGRAM.TARGET} 
  ${VALA_NAME}-c ${VALA_LONG_NAME} ${VALA_NAME}-py)

add_custom_target(run_test_${VALA_NAME}
  COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
    "GDB=1"
    "VERBOSE=1"
    ${CMAKE_CURRENT_BINARY_DIR}/test_${VALA_NAME}
    ${CMAKE_INSTALL_PREFIX}
    ${CMAKE_CURRENT_BINARY_DIR}/../vala
  DEPENDS ${TEST_PROGRAM.TARGET}
  )
