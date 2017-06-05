# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Srinivas Akkipeddi
# Creation Date: 12/09/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

include(rift_unittest)

set(PKG_NAME rwnetns)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs src plugins python test)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW.Library API Reference"
  DEST_DIR ${PKG_NAME})

