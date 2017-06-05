# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Max Beckett
# Creation Date: 7/14/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

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
