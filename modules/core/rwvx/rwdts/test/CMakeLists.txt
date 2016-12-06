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

set(subdirs yang lib)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

set(test_srcs rwdts_gtest.cc rwdts_gtest_ut.cc)
set(test_libs
  rwdts_api
  rwdts_router
  rwdts_api
  rwyang
  rwdtsproto
  rwmsg
  rwmsgbroker
  rwlib
  rwtrace
  rwtasklet
  rwmemlog
  rwlog
  rwsched
  rwdts_yang_gen
  dts_test_yang_gen
  rw_schema_pb
  kv_light_api
  rwdts_redis

  talloc
  nanomsg
  protobuf-c
  CoreFoundation
  dispatch
  BlocksRuntime
  ck
  glib-2.0)

set(REDIS_BIN_DIR ${CMAKE_INSTALL_PREFIX}/usr/bin/)
set(RWDTS_PYTHON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/python/)

rift_gtest(rwdts_gtest
  TEST_SRCS ${test_srcs}
  TEST_LIBS ${test_libs}
  TEST_ARGS --gtest_shuffle --gtest_repeat=1 --gtest_filter=-'*LONG*')

rift_gtest(rwdts_gtest_long
  LONG_UNITTEST_TARGET
  TEST_SRCS ${test_srcs}
  TEST_LIBS ${test_libs}
  DEPENDS ${test_deps}
  TEST_ARGS --gtest_shuffle --gtest_repeat=1)


set(test_srcs_redis rwdts_gtest_redis.cc)

set(test_srcs_sing_redis rwdts_gtest_no_cluster_redis.cc)

# Disabled by JLM in lpg_rpms as these tests should only
# apply to coreha
#rift_gtest(rwdts_gtest_redis
#TEST_SRCS ${test_srcs_redis}
#  TEST_LIBS ${test_libs}
#  TEST_ARGS --gtest_shuffle --gtest_repeat=1)
#
#rift_gtest(rwdts_gtest_no_cluster_redis
#  TEST_SRCS ${test_srcs_sing_redis}
#  TEST_LIBS ${test_libs}
#  TEST_ARGS --gtest_shuffle --gtest_repeat=1)

rift_py3test(rwdts_pytest
  TEST_ARGS
  ${RWDTS_PYTHON_DIR}/rwdts_pytest.py)

rift_py3test(rwdts_api_test
  TEST_ARGS
  ${RWDTS_PYTHON_DIR}/rwdts_api_test.py)
# Tons o' fun, RIFT-7085
rift_unittest(rwdts_gi_test-1
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_local_pub_sub
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-2
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_separate_pub_sub
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-3
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_registration_element
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-4
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_group
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-5
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_rpc
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-6
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_multiple_queries
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-7
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_na_on_prepare
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-8
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_elements_iter
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-9
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_multiple_responses
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-10
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_nack_on_prepare
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-11
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_rpc_nack
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-12
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_rpc_na
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-13
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_rpc_ack_and_nack
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-14
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_rpc_ack_and_na
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-15
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_precommit
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-16
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_both_commit_and_read
  ENVSET_ARGS FORCE=1)

# Disabled, RIFT-7559
#rift_unittest(rwdts_gi_test-17
#  TEST_ARGS
#  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_audit_subscriber
# ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-18
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_abort
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-19
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_rpc_blocks
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-20
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_simple_block
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-21
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_multiple_queries_in_blocks
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-22
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_multiple_block
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-23
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_pub_list
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-24
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_2_pub_sublist
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-25
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_1_pub_2_sublist
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-26
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_group_registration
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-27
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_group_reg_subquery
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-28
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_group_registration_commit
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-29
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_group_registration_nack
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-30
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_prepare_failure
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-31
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_precommit_failure
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-32
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_apply_exception
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-33
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_validate_exception
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-34
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_init_exception
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-35
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_group_registration_event_failure
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-36
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_group_registration_init_failure
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-37
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_simple_block_corrid
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-38
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_block_corrid_failure
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-39
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_sub_initiating_linked_create
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-40
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_safe_key
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-41
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_queue_key
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-42
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_unsafe_key
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-43
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_unsafe_pe
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-44
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_queue_pe
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-45
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_safe_pe
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-46
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_delete_success
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-47
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_delete_failure
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-48
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_unsafe_minikey
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-49
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_queue_minikey
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-50
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appdata_safe_minikey
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-51
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_no_prepare
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-52
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_delete_partial
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-53
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_delete_container
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-54
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_delete_wildcard
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-55
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_delete_reg_deeper_wildcard
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-56
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_delete_key
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-57
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_delete_container_wildcard
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-58
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_notification
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-59
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_2_pub
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-60
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_xact_create_wo_sub
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-61
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_xact_create_w_sub
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-62
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_prepare_fail
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-63
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_prepare_na
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-64
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_3_wildcards_pub_sub_with_cursor
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-65
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_wildcards_chain_pub_sub_with_cursor
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-66
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_appconf_prepare_exception
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-67
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_no_recovery_advance_without_cached_registrations
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-68
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_no_recovery_advance_with_no_registrations
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-69
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_xact_create_shallow_reroot
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-70
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_xact_create_deep_reroot
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-71
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_xact_update_deep_reroot
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-72
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_xact_update_shallow_reroot
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-73
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_xact_delete_deep_reroot
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-74
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_xact_delete_shallow_reroot
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-75
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_container_pub_in_list_pub
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-76
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_delete_partial
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-77
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_delete_container
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-78
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_delete_wildcard
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-79
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_delete_reg_deeper_wildcard
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-80
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_delete_key
  ENVSET_ARGS FORCE=1)

#rift_unittest(rwdts_gi_test-81
#  TEST_ARGS
#  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_recovery_test.py DtsTestCase.test_read_pub
#  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-82
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_return_payload
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-83
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_return_payload_sub
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-84
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_sub_read
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-85
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_deregister
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-86
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_reg_dereg
  ENVSET_ARGS FORCE=1)

rift_unittest(rwdts_gi_test-87
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/rwdts_gi_test.py DtsTestCase.test_multireg_wc_dereg
  ENVSET_ARGS FORCE=1)

add_custom_target(rwdts_gi_ut
  DEPENDS
    rwdts_gi_test-1
    rwdts_gi_test-2
    rwdts_gi_test-3
    rwdts_gi_test-4
    rwdts_gi_test-5
    rwdts_gi_test-6
    rwdts_gi_test-7
    rwdts_gi_test-8
    rwdts_gi_test-9
    rwdts_gi_test-10
    rwdts_gi_test-11
    rwdts_gi_test-12
    rwdts_gi_test-13
    rwdts_gi_test-14
    rwdts_gi_test-15
    rwdts_gi_test-16
#rwdts_gi_test-17
    rwdts_gi_test-18
    rwdts_gi_test-19
    rwdts_gi_test-20
    rwdts_gi_test-21
    rwdts_gi_test-22
    rwdts_gi_test-23
    rwdts_gi_test-24
    rwdts_gi_test-25
    rwdts_gi_test-26
    rwdts_gi_test-27
    rwdts_gi_test-28
    rwdts_gi_test-29
    rwdts_gi_test-30
    rwdts_gi_test-31
 rwdts_gi_test-32
    rwdts_gi_test-33
 rwdts_gi_test-34
    rwdts_gi_test-35
    rwdts_gi_test-36
    rwdts_gi_test-37
    rwdts_gi_test-38
    rwdts_gi_test-39
    rwdts_gi_test-40
    rwdts_gi_test-41
    rwdts_gi_test-42
    rwdts_gi_test-43
    rwdts_gi_test-44
    rwdts_gi_test-45
rwdts_gi_test-46
    rwdts_gi_test-47
    rwdts_gi_test-48
    rwdts_gi_test-49
    rwdts_gi_test-50
    rwdts_gi_test-51
    rwdts_gi_test-52
    rwdts_gi_test-53
    rwdts_gi_test-54
    rwdts_gi_test-55
    rwdts_gi_test-56
    rwdts_gi_test-57
    rwdts_gi_test-58
    rwdts_gi_test-59
    rwdts_gi_test-60
    rwdts_gi_test-61
rwdts_gi_test-62
    rwdts_gi_test-63
    rwdts_gi_test-64
    rwdts_gi_test-65
    rwdts_gi_test-66
    rwdts_gi_test-67
    rwdts_gi_test-68
    rwdts_gi_test-69
    rwdts_gi_test-70
    rwdts_gi_test-71
    rwdts_gi_test-72
    rwdts_gi_test-73
    rwdts_gi_test-74
    rwdts_gi_test-75
    rwdts_gi_test-76
    rwdts_gi_test-77
    rwdts_gi_test-78
    rwdts_gi_test-79
    rwdts_gi_test-80
    #rwdts_gi_test-81
    rwdts_gi_test-82
    rwdts_gi_test-83
    rwdts_gi_test-84
    rwdts_gi_test-85
    rwdts_gi_test-86
)

add_custom_target(rwdts_unittest
  DEPENDS
  rwdts_pytest
  rwdts_gtest
  rwdts_api_test
  )

## dts wrapper test
rift_unittest(rw_dts_wrapper_test-1
  TEST_ARGS
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/test_dts_wrapper.py
  )

