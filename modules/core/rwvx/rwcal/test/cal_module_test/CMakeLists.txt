# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Varun Prasad
# Creation Date: 21/01/2016
# 

cmake_minimum_required(VERSION 2.8)

install(
  PROGRAMS
    cal_module_test
  DESTINATION usr/rift/systemtest/cal_module_test
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    pytest/conftest.py
    pytest/cal_module_test.py
  DESTINATION usr/rift/systemtest/cal_module_test/pytest
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    racfg/cal_module_test.racfg
  DESTINATION
    usr/rift/systemtest/cal_module_test
    COMPONENT ${PKG_LONG_NAME})

