# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Balaji Rajappa 
# Creation Date: 8/25/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwtermio)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Add the subdirectories. TODO add test
##
set(subdirs 
  plugins
  src
)

rift_add_subdirs(SUBDIR_LIST ${subdirs})
