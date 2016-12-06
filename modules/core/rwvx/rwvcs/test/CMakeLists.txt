
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

include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/rwmain)

rift_gtest(unittest_rwvcs_rwzk
  TEST_SRCS rwvcs_rwzk_gtest.cpp
  TEST_LIBS
    rwschema_yang_gen
    rwlib
    rwsched
    rwvcs
    ck
    glib-2.0
)

rift_gtest(unittest_rwvcs_zk_api
  TEST_SRCS rwvcs_zk_gtest.cpp
  TEST_LIBS
    rwschema_yang_gen
    rwlib
    rwsched
    rwvcs
    ck
    glib-2.0
)

rift_gtest(unittest_rwvcs_callback
  TEST_SRCS rwvcs_callback_gtest.cpp
  TEST_LIBS
    rwvcs_zk-1.0
    rwvcs
)

rift_gtest(test_serf_client
  TEST_SRCS test_serf_client
  TEST_LIBS
    rwlib
    librwmain
    rwsched
    rwvcs

    glib-2.0
  TEST_ARGS --gtest_repeat=1)

rift_unittest(asyncio_test
  TEST_ARGS python3 ${CMAKE_CURRENT_SOURCE_DIR}/asyncio_test.py)


