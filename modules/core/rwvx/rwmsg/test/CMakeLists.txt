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

##
# Protobuf compile to C. Only pb-c decls for this one. no c++ or pb2c
##
set(proto_files test.proto)
rift_add_proto_target(
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  )

set(test_srcs rwmsg_gtest.cc rwmsg_broker_gtest.cc rwmsg_gtest_c.c ${proto_c_files})
set(test_libs
  rwmsgbroker
  rwvcs
  rwtasklet
  ${rwmsg_link_libs})

set(test_deps rwmsg rwmsgbroker rwsched rwlog)
##
# Unit test targets
###
## The following target runs the unittests
## Need to crank the --gtest_repeat for the nightly...
###

# Same vehicle linked against libdispatch-based scheduler's toy api instead
set(PERFTOOL_C_SOURCES rwmsg_perftool.c)
add_executable(rwmsg_perftool ${PERFTOOL_C_SOURCES})
target_link_libraries(rwmsg_perftool
  rwmsgbroker
  rwvcs
  rwtasklet
  ${rwmsg_link_libs})
add_dependencies(rwmsg_perftool rwmsg rwmsgbroker rwsched rwlog)

rift_gtest(rwmsg_gtest
  TEST_SRCS ${test_srcs}
  TEST_LIBS ${test_libs}
  DEPENDS ${test_deps}
  TEST_ARGS --gtest_shuffle --gtest_repeat=1 --gtest_filter='*-*Lotta*:*Blk10:*Sleep*:*Perftool*:*Big*:*Close*:*PbCli*:*LONG*'
)
# commented out by JLM since it hangs almost every time
rift_gtest(unittest_rwmsg_long
  LONG_UNITTEST_TARGET
  TEST_SRCS ${test_srcs}
  TEST_LIBS ${test_libs}
  DEPENDS ${test_deps}
  TEST_ARGS --gtest_shuffle --gtest_repeat=1
)

#rift_unittest(unittest_rwmsg_operf
#  LONG_UNITTEST_TARGET
#  TEST_EXE env
#  TEST_ARGS "NOTHING=nothing bash -c '( operf -g --event=CPU_CLK_UNHALTED:9999999 ${CMAKE_CURRENT_BINARY_DIR}/test/rwmsg_gtest --gtest_filter=RWMsgBroker.LottaRawRWSSerQ && opreport -c -g )'"
#  DEPENDS rwmsg_gtest)

