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
# Author(s): Srinivas Akkipeddi
# Creation Date: 03/31/2014
# 

# This module looks for libnl
#  and sets the following variables
# RIFT_LIBNL_FOUND         - set to 1 if module(s) exist
# RIFT_LIBNL_LIBRARIES     - only the libraries (w/o the '-l')
# RIFT_LIBNL_LIBRARY_DIRS  - the paths of the libraries (w/o the '-L')
# RIFT_LIBNL_LDFLAGS       - all required linker flags
# RIFT_LIBNL_LDFLAGS_OTHER - all other linker flags
# RIFT_LIBNL_INCLUDE_DIRS  - the '-I' preprocessor flags (w/o the '-I')
# RIFT_LIBNL_CFLAGS        - all required cflags
# RIFT_LIBNL_CFLAGS_OTHER  - the other compiler flags

cmake_minimum_required(VERSION 2.8)

include(FindPkgConfig)

if(NOT PKG_CONFIG_FOUND)
  message(FATAL_ERROR "PKG CONFIG NOT FOUND")
endif()

# This cmake function searches the .pc files 
# and sets the above mentioned varibales
pkg_check_modules(RIFT_LIBNL libnl-3.0)

set(DEBUG_FindRIFT_LIBNL 0)
if(DEBUG_FindRIFT_LIBNL)
  message("RIFT_LIBNL_FOUND = ${RIFT_LIBNL_FOUND}")
  message("RIFT_LIBNL_LIBRARIES = ${RIFT_LIBNL_LIBRARIES}")
  message("RIFT_LIBNL_LIBRARY_DIRS = ${RIFT_LIBNL_LIBRARY_DIRS}")
  message("RIFT_LIBNL_LDFLAGS = ${RIFT_LIBNL_LDFLAGS}")
  message("RIFT_LIBNL_LDFLAGS_OTHER = ${RIFT_LIBNL_LDFLAGS_OTHER}")
  message("RIFT_LIBNL_INCLUDE_DIRS = ${RIFT_LIBNL_INCLUDE_DIRS}")
  message("RIFT_LIBNL_CFLAGS = ${RIFT_LIBNL_CFLAGS}")
  message("RIFT_LIBNL_CFLAGS_OTHER = ${RIFT_LIBNL_CFLAGS_OTHER}")
endif()