# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwtrace)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Add the subdirectories
##
set(subdirs test src)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen 
  DOXY_NAME "RW.Trace QuickStart and API Reference"
  DOXY_FILES rwtrace.h
  DEST_DIR ${PKG_NAME}
)

