# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Vijay Nag
# Creation Date: 09/20/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)


set(subdir
    deps
    src)

add_subdirectory(deps)

add_subdirectory(src)

