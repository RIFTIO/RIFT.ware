# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Austin Cormier
# Creation Date: 01/04/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Add the subdirectories
##
set(subdirs
  yang)

rift_add_subdirs(SUBDIR_LIST ${subdirs})
