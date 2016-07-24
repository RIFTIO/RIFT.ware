# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Rajesh Velandy
# Creation Date: 2014/09/18
# 

cmake_minimum_required(VERSION 2.8)


##
# Yang files that are used by  the DTS Member API unit tests
# These are required to compile the unit tests.
##
rift_files_prepend_path(
  dts_test_yang_src
  ${CMAKE_CURRENT_SOURCE_DIR}
  testdts-rw-base.yang
  testdts-rw-fpath.yang
  testdts-rw-stats.yang
  dts-test.yang               ## A test yang file to test DTS member APIs
)

##
# Yang parsing needed for the tests
rift_add_yang_target(
  TARGET dts_test_yang
  YANG_FILES ${dts_test_yang_src}
  LIBRARIES rwdts_yang_gen
  NO_INSTALL
)
