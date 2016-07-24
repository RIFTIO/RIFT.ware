# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Joshua Downer
# Creation Date: 10/01/2015
# 

cmake_minimum_required(VERSION 2.8)

install(
  PROGRAMS
    launchpad.py
    DESTINATION demos
  COMPONENT ${PKG_LONG_NAME}
  )

install(
  FILES
    racfg/launchpad_module_test.racfg
  DESTINATION
    usr/rift/systemtest/launchpad
  COMPONENT ${PKG_LONG_NAME}
  )

install(
  FILES
    pytest/lp_test.py
  DESTINATION
    usr/rift/systemtest/pytest/launchpad
  COMPONENT ${PKG_LONG_NAME}
  )

install(
  PROGRAMS
    launchpad_module_test
  DESTINATION
    usr/rift/systemtest/launchpad
  COMPONENT ${PKG_LONG_NAME}
  )

install(
  PROGRAMS
    launchpad_recovery
  DESTINATION
    usr/rift/systemtest/launchpad
  COMPONENT ${PKG_LONG_NAME}
  )

#rift_py3test(rwmano_utest
#  TEST_ARGS
#  ${CMAKE_CURRENT_SOURCE_DIR}/mano_ut.py
#  )

#rift_py3test(launchpad_recovery
# TEST_ARGS
# ${CMAKE_CURRENT_SOURCE_DIR}/launchpad_recovery
# )

rift_py3test(utest_rwmonitor
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/utest_rwmonitor.py
  )

rift_py3test(utest_rwnsm
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/utest_rwnsm.py
  )

rift_py3test(tosca_ut
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/tosca_ut.py
  )
