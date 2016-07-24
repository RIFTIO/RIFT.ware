
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/rwmain)

rift_gtest(unittest_rwvcs_rwzk
  TEST_SRCS rwvcs_rwzk_gtest.cpp
  TEST_LIBS
    rwcal_api
    rwschema_yang_gen
    rwlib
    rwsched
    rwvcs
    ck
    glib-2.0
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


