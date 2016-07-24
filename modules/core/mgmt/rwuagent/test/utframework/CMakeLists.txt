# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs yang)
rift_add_subdirs(${subdirs})

rift_install_python_plugin(testtasklet testtasklet.py)
rift_install_python_plugin(testdriver_tasklet testdriver_tasklet.py)

install(
  PROGRAMS
    mgmt_tbed.py
  DESTINATION
    demos
  COMPONENT
    ${PKG_LONG_NAME}
)

rift_python_install_tree(
    FILES
      rift/tasklets/uagenttbed/__init__.py
      rift/tasklets/uagenttbed/testbed_tests.py
      rift/tasklets/uagenttbed/testtasklet.py
      rift/tasklets/uagenttbed/testdriver_tasklet.py
    PYTHON3_ONLY
    COMPONENT
      ${PKG_LONG_NAME}
)

install(FILES
  mgmtagt_tbed_schemas.txt
  DESTINATION
    usr/data/manifest
  COMPONENT
    ${PKG_LONG_NAME}
)

#rift_unittest(rwrest_module_test-1
#  LONG_UNITTEST_TARGET
#  TEST_ARGS
#    python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwrest_module_test.py
#)

install(
  PROGRAMS
    mgmt_agent_test.sh
  DESTINATION
     usr/rift/systemtest/mgmtagt-testbed
  COMPONENT
     ${PKG_LONG_NAME}
)

install(
  FILES
    mgmtagt_testbed.racfg
  DESTINATION
    usr/rift/systemtest/mgmtagt-testbed
  COMPONENT
    ${PKG_LONG_NAME}
)
