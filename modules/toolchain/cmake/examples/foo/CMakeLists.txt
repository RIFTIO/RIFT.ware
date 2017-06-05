# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 11/22/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# To see this example at work follow these steps:
#
# mkdir build
# cd build
# cmake ../
# make
##

##
# The following code adds the subdirectories 
# for the compilation.
##
set(subdirs app lib)
foreach(subdir ${subdirs})
  add_subdirectory(${subdir})
endforeach(subdir)

