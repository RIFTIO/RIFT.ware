# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Balaji Rajappa 
# Creation Date: 08/25/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Add the subdirectories
##
set(subdirs
  rwtermio-c)

rift_add_subdirs(SUBDIR_LIST ${subdirs})
