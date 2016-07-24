# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

# This module looks for LIBPEAS and sets the following variables
# RIFT_LIBPEAS_FOUND         - set to 1 if module(s) exist
# RIFT_LIBPEAS_LIBRARIES     - only the libraries (w/o the '-l')
# RIFT_LIBPEAS_LIBRARY_DIRS  - the paths of the libraries (w/o the '-L')
# RIFT_LIBPEAS_LDFLAGS       - all required linker flags
# RIFT_LIBPEAS_LDFLAGS_OTHER - all other linker flags
# RIFT_LIBPEAS_INCLUDE_DIRS  - the '-I' preprocessor flags (w/o the '-I')
# RIFT_LIBPEAS_CFLAGS        - all required cflags
# RIFT_LIBPEAS_CFLAGS_OTHER  - the other compiler flags

cmake_minimum_required(VERSION 2.8)

# Find PkgConfig
include(FindPkgConfig)
if(NOT PKG_CONFIG_FOUND)
  message(FATAL_ERROR "PKG CONFIG NOT FOUND")
endif()

# This cmake function searches the .pc files 
# and sets the above mentioned varibales
pkg_check_modules(RIFT_LIBPEAS libpeas-1.0)

# need an additional include dir
execute_process(
  COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=prefix libpeas-1.0
  OUTPUT_VARIABLE _prefix
  RESULT_VARIABLE _result
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

if(NOT _result)
  set(RIFT_LIBPEAS_INCLUDE_DIRS 
      ${RIFT_LIBPEAS_INCLUDE_DIRS} ${_prefix}/include/libpeas-1.0)
endif()

set(DEBUG_FindRIFT_LIBPEAS 0)
if(DEBUG_FindRIFT_LIBPEAS)
  message("RIFT_LIBPEAS_FOUND = ${RIFT_LIBPEAS_FOUND}")
  message("RIFT_LIBPEAS_LIBRARIES = ${RIFT_LIBPEAS_LIBRARIES}")
  message("RIFT_LIBPEAS_LIBRARY_DIRS = ${RIFT_LIBPEAS_LIBRARY_DIRS}")
  message("RIFT_LIBPEAS_LDFLAGS = ${RIFT_LIBPEAS_LDFLAGS}")
  message("RIFT_LIBPEAS_LDFLAGS_OTHER = ${RIFT_LIBPEAS_LDFLAGS_OTHER}")
  message("RIFT_LIBPEAS_INCLUDE_DIRS = ${RIFT_LIBPEAS_INCLUDE_DIRS}")
  message("RIFT_LIBPEAS_CFLAGS = ${RIFT_LIBPEAS_CFLAGS}")
  message("RIFT_LIBPEAS_CFLAGS_OTHER = ${RIFT_LIBPEAS_CFLAGS_OTHER}")
endif()
