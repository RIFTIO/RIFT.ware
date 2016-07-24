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
  log_test_yang_src
  ${CMAKE_CURRENT_SOURCE_DIR}
  rw-logtest.yang
)

rift_files_prepend_path(
  dynschema_log_test_yang_src
  ${CMAKE_CURRENT_SOURCE_DIR}
  rwdynschema-logtest.yang
)

##
# Yang parsing needed for the tests
rift_add_yang_target(
  TARGET log_test_yang
  YANG_FILES ${log_test_yang_src}
  LIBRARIES
    rwschema_yang_gen
  NO_INSTALL
)

rift_add_yang_target(
  TARGET dynschema_log_test_yang
  YANG_FILES ${dynschema_log_test_yang_src}
  LIBRARIES 
    rwschema_yang_gen
  NO_INSTALL
)
