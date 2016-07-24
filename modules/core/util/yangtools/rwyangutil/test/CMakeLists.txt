# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Arun Muralidharan
# Creation Date: 2015/10/01
# 

cmake_minimum_required(VERSION 2.8)

set(test_srcs
  rwyangutil_test.cc
)

find_library(boost_filesystem_path NAMES boost_filesystem)
if (NOT boost_filesystem_path)
  message (FATAL_ERROR "Could not find library ${boost_filesystem_path}")
endif ()
find_library(boost_system_path NAMES boost_system)
if (NOT boost_system_path)
  message (FATAL_ERROR "Could not find library ${boost_system_path}")
endif ()

set(test_libs
  ${boost_filesystem_path}
  ${boost_system_path}
  rwyangutil_lib
)

rift_gtest(rwyangutil_utest
  TEST_SRCS ${test_srcs}
  TEST_LIBS ${test_libs}
  TEST_ARGS --gtest_shuffle --gtest_repeat=1
)

# install test northbound listing
install(FILES
    test_northbound_listing.txt
    test_northbound_widening_listing.txt
  DESTINATION
    usr/data/manifest
  COMPONENT
    ${PKG_LONG_NAME}
  )
