# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs yang)
rift_add_subdirs(${subdirs})

rift_install_python_plugin(
  testtasklet testtasklet.py
  COMPONENT
    ${INSTALL_COMPONENT}
)
rift_install_python_plugin(
  testdriver_tasklet testdriver_tasklet.py
  COMPONENT
    ${INSTALL_COMPONENT}
)

install(
  PROGRAMS
    mgmt_tbed.py
  DESTINATION
    demos
  COMPONENT
    ${INSTALL_COMPONENT}
)

rift_python_install_tree(
    FILES
      rift/tasklets/uagenttbed/__init__.py
      rift/tasklets/uagenttbed/testbed_tests.py
      rift/tasklets/uagenttbed/testtasklet.py
      rift/tasklets/uagenttbed/testdriver_tasklet.py
    PYTHON3_ONLY
    COMPONENT
      ${INSTALL_COMPONENT}
)

install(FILES
  mgmtagt_tbed_schemas.txt
  DESTINATION
    usr/data/manifest
  COMPONENT
    ${INSTALL_COMPONENT}
)

#rift_unittest(rwrest_module_test-1
#  LONG_UNITTEST_TARGET
#  TEST_ARGS
#    python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwrest_module_test.py
#)

install(
  PROGRAMS
    mgmt_agent_test.sh
    mgmt_agent_test_cmd.sh
  DESTINATION
     usr/rift/systemtest/mgmtagt-testbed
  COMPONENT
     ${INSTALL_COMPONENT}
)

install(
  FILES
    mgmtagt_testbed.racfg
  DESTINATION
    usr/rift/systemtest/mgmtagt-testbed
  COMPONENT
    ${INSTALL_COMPONENT}
)
