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

#minimum required cmake path
cmake_minimum_required(VERSION 2.8)
cmake_policy(VERSION 2.8)

if(NOT DEFINED ENV{RIFT_BUILD})
    message(FATAL_ERROR "RIFT_BUILD must be set")
endif()

if(NOT DEFINED ENV{RIFT_PLATFORM})
    message(FATAL_ERROR "RIFT_PLATFORM must be set")
endif()

set(RIFT_CMAKE_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(RIFT_CMAKE_BIN_DIR ${CMAKE_CURRENT_LIST_DIR}/../bin ABSOLUTE)

#path for searching cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RIFT_CMAKE_MODULE_DIR}/granite)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RIFT_CMAKE_MODULE_DIR}/vala)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RIFT_CMAKE_MODULE_DIR}/helper)

# If we're in developer mode, then use the patched CMAKE RPM for proper build cache
# otherwise use the official CPackRPM version.
if(NOT_DEVELOPER_BUILD MATCHES "TRUE"
  OR NOT_DEVELOPER_BUILD MATCHES "true"
  OR NOT_DEVELOPER_BUILD MATCHES "True"
  OR NOT_DEVELOPER_BUILD MATCHES "1")
  set(RIFT_DEVELOPER_BUILD FALSE)

  # Use a patched version of CPACKRPM to address this issue: http://public.kitware.com/Bug/view.php?id=14782
  # Otherwise the @ character in confd install causes issues
  if ($ENV{RIFT_PLATFORM} MATCHES "fc20")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RIFT_CMAKE_MODULE_DIR}/patches/fc20_non_developer)
  endif()

else()
  set(RIFT_DEVELOPER_BUILD TRUE)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${RIFT_CMAKE_MODULE_DIR}/patches)
endif()

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  message(FATAL_ERROR "CMAKE_INSTALL_PREFIX must be provided")
endif(NOT DEFINED CMAKE_INSTALL_PREFIX)

set(RIFT_BINDIR bin)
set(RIFT_LIBDIR lib)
set(RIFT_INCLUDEDIR include)
set(RIFT_PYDIR ${RIFT_LIBDIR}/python2.7/site-packages)

set(RIFT_INSTALL ${CMAKE_INSTALL_PREFIX}/usr)
set(RIFT_INSTALL_BINDIR ${RIFT_INSTALL}/${RIFT_BINDIR})
set(RIFT_INSTALL_LIBDIR ${RIFT_INSTALL}/${RIFT_LIBDIR})
set(RIFT_INSTALL_PYDIR ${RIFT_INSTALL}/${RIFT_PYDIR})
set(RIFT_INSTALL_VARDIR ${CMAKE_INSTALL_PREFIX}/var)

set(cl "import sys; print('%d.%d' % sys.version_info[:2])")
execute_process(COMMAND python3 -c "${cl}" OUTPUT_VARIABLE temp OUTPUT_STRIP_TRAILING_WHITESPACE)
set(RIFT_PYTHON3 ${temp})
set(RIFT_PYTHON2 2.7)
set(RIFT_SUPPORT_PYTHON2 TRUE)

#enable the verbose option
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(RIFT_CACHE_BASE_DIR
  "/net/jenkins/localdisk/jenkins/RIFT_BCACHE/$ENV{RIFT_PLATFORM}")
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

if(NOT DEFINED RELEASE_NUMBER)
  message("RELEASE_NUMBER not specified.  Using 9")
  set(RELEASE_NUMBER "9")
endif()

if(NOT DEFINED BUILD_NUMBER)
  message("BUILD_NUMBER not specified.  Using 999")
  set(BUILD_NUMBER "999")
endif()

set(RIFT_VERSION "${RELEASE_NUMBER}.${BUILD_NUMBER}")

# set the build cache type
# Debug builds with and without coverage
# must have different build caches
if(COVERAGE_BUILD MATCHES TRUE)
  set(BUILD_CACHE_TYPE ${CMAKE_BUILD_TYPE}_Coverage)
elseif(NOT ${RIFT_DEVELOPER_BUILD})
  set(BUILD_CACHE_TYPE ${CMAKE_BUILD_TYPE}_FILES)
elseif(RIFT_AGENT_BUILD MATCHES CONFD_FULL)
  set(BUILD_CACHE_TYPE ${CMAKE_BUILD_TYPE})
else()
  set(BUILD_CACHE_TYPE ${CMAKE_BUILD_TYPE}_${RIFT_AGENT_BUILD})
endif()
message("Build Cache Type: ${BUILD_CACHE_TYPE}")
