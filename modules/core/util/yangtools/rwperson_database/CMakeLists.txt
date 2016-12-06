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
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

cmake_minimum_required(VERSION 2.8)

set(GI_NAME RwpersonDB)
set(GI_VERSION 1.0)
set(GI_NAMESPACE RwpersonDB)

##
# Yang parsing needed for google tests
##
rift_add_yang_target(
  TARGET rwperson_database_yang
  YANG_FILES rwperson-db.yang
  LIBRARIES rwext_yang_gen ietf_yang_gen
  NO_LIB_COMPONENT 
  COMPONENT ${INSTALL_COMPONENT}
)

##
# Add the RwpersonDB python test
##
rift_py3test(rwperson_database_pytest
  TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/rwperson_database_pytest.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS rwperson_database_yang
  )

rift_unittest(rwperson_database_pytest3
  TEST_ARGS python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwperson_database_pytest.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS rwperson_database_yang)

##
# Add the RwpersonDB lua test
##
rift_luatest(rwperson_database_luatest
  TEST_ARGS rwperson_database_luatest.lua
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS rwperson_database_yang
  )

##
# Add the RwpersonDB c++ gtest
##
set(testlibs
  rwyang
  rwlib
  rwtrace
  protobuf-c
  rwperson_database_yang_gen
  CoreFoundation
)

rift_gtest(rwperson_database_gtest
  ADD_SETUP
  TEST_SRCS rwperson_database_gtest.cpp
  TEST_LIBS ${testlibs}
  DEPENDS rwperson_database_yang
)
