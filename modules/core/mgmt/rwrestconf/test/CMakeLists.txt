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
# Author(s): Max Beckett
# Creation Date: 7/14/2015
# 

rift_add_yang_target(
  TARGET rwrestconf_test_yang
  YANG_FILES
  vehicle-a.yang
  vehicle-augment-a.yang
  )

# specify each test file RIFT-7085
rift_unittest(rw_restconf_test-1
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/test_rest.py
  )

rift_unittest(rw_restconf_test-2
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/test_rest_get.py
  )

rift_unittest(rw_restconf_test-4
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/test_rest_put.py
  )

rift_unittest(rw_restconf_test-3
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/test_stream.py
  )

rift_add_subdirs(
  SUBDIR_LIST
    notification
    restconf_systest
  )
