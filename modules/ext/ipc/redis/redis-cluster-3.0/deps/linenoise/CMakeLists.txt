# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Vijay Nag
# Creation Date: 09/20/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_C_FLAGS "-Os -Wall")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

add_executable(linenoise_example
              linenoise.c
              example.c)

