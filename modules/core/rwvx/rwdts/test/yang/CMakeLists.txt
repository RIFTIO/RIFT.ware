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


