# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Rajesh Ramankutty
# Creation Date: 2015/05/08
# 

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
