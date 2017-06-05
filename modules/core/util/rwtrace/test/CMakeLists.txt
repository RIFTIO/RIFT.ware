# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Converted unit test target (runs with GTest and Celero)
##
rift_gtest(rwtrace_gtest
  TEST_SRCS rwtrace_gtest.cc
  TEST_LIBS 
    rwtrace
    rwlib
  DEPENDS rwtrace rwlib
)
