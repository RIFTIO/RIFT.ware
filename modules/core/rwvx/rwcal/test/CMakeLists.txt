# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 06/27/2014
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs cal_module_test)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# Unit test target
##

rift_gtest(unittest_rwcal_zk
  TEST_SRCS rwcal_zk_gtest.cpp
  TEST_LIBS
    rwcal_api
    rwvcs
)

# rift_gtest(unittest_rwcal_cloud
#   TEST_SRCS rwcal_cloud_gtest.cpp
#   TEST_LIBS
#     rwcal_api
#     rwcal_yang_gen
# )

rift_gtest(unittest_rwcal_callback
  TEST_SRCS rwcal_callback_gtest.cpp
  TEST_LIBS
    rwcal-1.0
    rwcal_api
)

##
# Add the basic plugin python test
##
#rift_py3test(openstack_cal_tests
#  LONG_UNITTEST_TARGET
#  TEST_ARGS -m pytest --junit-xml=${RIFT_UNITTEST_DIR}/openstack_cal/unittest.xml #${CMAKE_CURRENT_SOURCE_DIR}/test_rwcal_openstack_pytest.py
#)


add_executable(rwcal_dump rwcal_dump.cpp)
target_link_libraries(rwcal_dump
  rwcal_api
  rwlib
  rwyang
  rwcal_yang_gen
  CoreFoundation
  glib-2.0
  protobuf-c
)

# added for 4.0
install(
  FILES 
    RIFT.ware-ready.py 
    openstack_resources.py
  DESTINATION usr/bin
  COMPONENT ${PKG_LONG_NAME}
)

