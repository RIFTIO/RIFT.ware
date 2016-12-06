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

rift_generate_python_log_yang(
  LOG_CATEGORY_NAME rw-logtest-log
  START_EVENT_ID 51000
  OUT_YANG_FILE_VAR rw_logtest_log_file
  )

rift_add_yang_target(
  TARGET rw_logtest_yang
  YANG_FILES ${rw_logtest_log_file}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
    rwmanifest_yang_gen
)

# RIFT-5016
rift_gtest(rwlog_gtest
  TEST_SRCS rwlog_gtest.cc
  TEST_LIBS
    rwlog
    log_test_yang_gen
    rwschema_yang_gen
    rwlib
    rt
    glib-2.0
  DEPENDS rwlog glib-2.0
)

##
# Add the rwlog python test
##
rift_pytest(rwlog_pytest
  TEST_ARGS rwlog_pytest.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS rwlog rwgeneric_yang
)

rift_py3test(log_tasklet_test
  TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/log_tasklet_test.py
  DEPENDS rwlog rwgeneric_yang rw_logtest_yang
)

