# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Rajesh Ramankutty
# Creation Date: 2015/05/08
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Unit test target
##

rift_gtest(unittest_rwshell
  TEST_SRCS rwshell_gtest.cpp
  TEST_LIBS
    rwshell-1.0
    rwshell_api
    rwvcs
)
