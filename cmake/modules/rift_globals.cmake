# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

#minimum required cmake path
cmake_minimum_required(VERSION 2.8)
cmake_policy(VERSION 2.8)

#path for searching cmake modules
if(DEFINED PROJECT_TOP_DIR)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_TOP_DIR}/cmake/modules/granite)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_TOP_DIR}/cmake/modules/vala)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_TOP_DIR}/cmake/modules/helper)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_TOP_DIR}/cmake/modules/patches)
  set(CMAKE_INSTALL_PREFIX ${PROJECT_TOP_DIR}/.install)

  set(RIFT_BINDIR bin)
  set(RIFT_LIBDIR lib)
  set(RIFT_INCLUDEDIR include)
  set(RIFT_PYDIR ${RIFT_LIBDIR}/python2.7/site-packages)

  set(RIFT_INSTALL ${CMAKE_INSTALL_PREFIX}/usr)
  set(RIFT_INSTALL_BINDIR ${RIFT_INSTALL}/${RIFT_BINDIR})
  set(RIFT_INSTALL_LIBDIR ${RIFT_INSTALL}/${RIFT_LIBDIR})
  set(RIFT_INSTALL_PYDIR ${RIFT_INSTALL}/${RIFT_PYDIR})
  set(RIFT_INSTALL_VARDIR ${CMAKE_INSTALL_PREFIX}/var)

  set(RIFT_PYTHON3 3.3)
  set(RIFT_PYTHON2 2.7)
  set(RIFT_SUPPORT_PYTHON2 TRUE)
endif(DEFINED PROJECT_TOP_DIR)

#enable the verbose option
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(RIFT_CACHE_BASE_DIR "")
set(RIFT_VENDOR_NAME RIFTio)

# validate the cmake build type
if(CMAKE_BUILD_TYPE)
  # allow only Release and RelWithDebInfo releases
  # string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)
  set(build_type ${CMAKE_BUILD_TYPE})
  if(build_type MATCHES "Debug|Release|RelWithDebInfo")
    message("Build type: ${CMAKE_BUILD_TYPE} @ ${CMAKE_CURRENT_SOURCE_DIR}")
  else(build_type MATCHES "Debug|Release|RelWithDebInfo")
    message("Invalid build type '${CMAKE_BUILD_TYPE}'")
    message("Usage:")
    message("    cmake [-DCMAKE_BUILD_TYPE=Debug|")
    message("           -DCMAKE_BUILD_TYPE=Release|")
    message("           -DCMAKE_BUILD_TYPE=RelWithDebInfo]")
    message(FATAL_ERROR "")
  endif(build_type MATCHES "Debug|Release|RelWithDebInfo")
else()
  # set the default build type to debug
  set(CMAKE_BUILD_TYPE Debug)
  message("Build type: unspecified - defaulting to Debug @ ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# set the build cache type
# Debug builds with and without coverage
# must have different build caches
if(COVERAGE_BUILD MATCHES TRUE)
  set(BUILD_CACHE_TYPE ${CMAKE_BUILD_TYPE}_Coverage)
elseif(NOT_DEVELOPER_BUILD MATCHES TRUE)
  set(BUILD_CACHE_TYPE ${CMAKE_BUILD_TYPE}_FILES)
elseif(RIFT_AGENT_BUILD MATCHES CONFD_FULL)
  set(BUILD_CACHE_TYPE ${CMAKE_BUILD_TYPE})
else()
  set(BUILD_CACHE_TYPE ${CMAKE_BUILD_TYPE}_${RIFT_AGENT_BUILD})
endif()
message("Build Cache Type: ${BUILD_CACHE_TYPE}")
