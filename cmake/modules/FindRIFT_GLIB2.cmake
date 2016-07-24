# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

# This module looks for gobject-introspection,
#  and sets the following variables
# RIFT_GLIB2_FOUND         - set to 1 if module(s) exist
# RIFT_GLIB2_LIBRARIES     - only the libraries (w/o the '-l')
# RIFT_GLIB2_LIBRARY_DIRS  - the paths of the libraries (w/o the '-L')
# RIFT_GLIB2_LDFLAGS       - all required linker flags
# RIFT_GLIB2_LDFLAGS_OTHER - all other linker flags
# RIFT_GLIB2_INCLUDE_DIRS  - the '-I' preprocessor flags (w/o the '-I')
# RIFT_GLIB2_CFLAGS        - all required cflags
# RIFT_GLIB2_CFLAGS_OTHER  - the other compiler flags

cmake_minimum_required(VERSION 2.8)

include(FindPkgConfig)

if(NOT PKG_CONFIG_FOUND)
  message(FATAL_ERROR "PKG CONFIG NOT FOUND")
endif()

# This cmake function searches the .pc files 
# and sets the above mentioned varibales
pkg_check_modules(RIFT_GLIB2 glib-2.0)

set(DEBUG_FindRIFT_GLIB2 0)
if(DEBUG_FindRIFT_GLIB2)
  message("RIFT_GLIB2_FOUND = ${RIFT_GLIB2_FOUND}")
  message("RIFT_GLIB2_LIBRARIES = ${RIFT_GLIB2_LIBRARIES}")
  message("RIFT_GLIB2_LIBRARY_DIRS = ${RIFT_GLIB2_LIBRARY_DIRS}")
  message("RIFT_GLIB2_LDFLAGS = ${RIFT_GLIB2_LDFLAGS}")
  message("RIFT_GLIB2_LDFLAGS_OTHER = ${RIFT_GLIB2_LDFLAGS_OTHER}")
  message("RIFT_GLIB2_INCLUDE_DIRS = ${RIFT_GLIB2_INCLUDE_DIRS}")
  message("RIFT_GLIB2_CFLAGS = ${RIFT_GLIB2_CFLAGS}")
  message("RIFT_GLIB2_CFLAGS_OTHER = ${RIFT_GLIB2_CFLAGS_OTHER}")
endif()

