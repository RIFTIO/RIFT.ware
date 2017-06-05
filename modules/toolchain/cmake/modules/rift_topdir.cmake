# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

# this should be included by the CMakeLists.txt in the root directory
# of every git submodule

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git AND NOT ${RIFT_NON_SUBMODULE_PROJECT})
  message(FATAL_ERROR "Included rift_topdir but not in a submodule root directory")
endif()

include(rift_globals NO_POLICY_SCOPE)

#cmake_policy(SET CMP0017 NEW)
