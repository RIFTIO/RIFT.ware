# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Rajesh Velandy
# Creation Date: 2014/09/18
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

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
  WITHOUT_GI
  NO_INSTALL
)

## dts wrapper test

rift_generate_python_log_yang(
  LOG_CATEGORY_NAME
    rw-dts-wrapper-test-log
  START_EVENT_ID
    52000
  OUT_YANG_FILE_VAR
    test_log_category_yang
)

rift_add_yang_target(
  TARGET
    rw_dts_wrapper_test
  YANG_FILES
    ${test_log_category_yang}
    rw-dts-wrapper-test.yang
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
)


