# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

# this should be included by the CMakeLists.txt in the root directory
# of every git submodule

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git AND NOT ${RIFT_NON_SUBMODULE_PROJECT})
  message(FATAL_ERROR "Included rift_topdir but not in a submodule root directory")
endif()

get_filename_component(SEARCH_PATH ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
set(done 0)
while(NOT ${done} AND SEARCH_PATH)
  find_path(PROJECT_TOP_DIR .gitmodules ${SEARCH_PATH})
  if(PROJECT_TOP_DIR)
    set(done 1)
  endif(PROJECT_TOP_DIR)
  get_filename_component(SEARCH_PATH ${SEARCH_PATH}/../ ABSOLUTE)
endwhile(NOT ${done} AND SEARCH_PATH)

if (NOT PROJECT_TOP_DIR)
  message(FATAL_ERROR "Couldn't find project root dir")
endif(NOT PROJECT_TOP_DIR)

set(CMAKE_MODULE_PATH ${PROJECT_TOP_DIR}/cmake/modules)
include(rift_globals)

#cmake_policy(SET CMP0017 NEW)
