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
  WITHOUT_GI
  NO_INSTALL
)

rift_add_yang_target(
  TARGET dynschema_log_test_yang
  YANG_FILES ${dynschema_log_test_yang_src}
  LIBRARIES 
    rwschema_yang_gen
  WITHOUT_GI
  NO_INSTALL
)
