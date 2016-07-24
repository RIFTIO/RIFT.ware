# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
  COMPONENT ${PKG_LONG_NAME}
)

##
# Add the RwpersonDB python test
##
rift_pytest(rwperson_database_pytest
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
