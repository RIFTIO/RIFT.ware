# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 12/2/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Add the subdirectories
##
set(subdirs src test plugins)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
