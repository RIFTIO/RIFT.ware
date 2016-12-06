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
# Author(s): Austin Cormier
# Creation Date: 07/28/2014
# 

include(CMakeParseArguments)

set(RIFT_SYSTEMTEST_DIR $ENV{RIFT_SYSTEM_TEST})

##
# rift_systemtest(target
#                 [ LONG_SYSTEMTEST_TARGET ]
#                 TEST_EXE <test-program-name>
#                 TEST_ARGS <arguments-to-test-program>
#                 DEPENDS <target-dependencies>)
##
function(rift_systemtest target)
  set(parse_options LONG_SYSTEMTEST_TARGET)
  set(parse_onevalargs TEST_EXE)
  set(parse_multivalueargs TEST_ARGS DEPENDS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(ARGS_LONG_SYSTEMTEST_TARGET)
    set(RIFT_LONG_SYSTEMTEST_TARGET_LIST ${RIFT_LONG_SYSTEMTEST_TARGET_LIST} 
      ${target} CACHE INTERNAL "Long Systemtest Target List")
  else(ARGS_LONG_SYSTEMTEST_TARGET)
    set(RIFT_SYSTEMTEST_TARGET_LIST ${RIFT_SYSTEMTEST_TARGET_LIST} 
      ${target} CACHE INTERNAL "Systemtest Target List")
  endif(ARGS_LONG_SYSTEMTEST_TARGET)

  set(working_directory ${CMAKE_CURRENT_SOURCE_DIR})

  # Set up the custom target for the systemtest
  set(outputDir ${RIFT_SYSTEMTEST_DIR})
  add_custom_target(${target}
    COMMAND mkdir -p ${outputDir}
    COMMAND ${RIFT_CMAKE_BIN_DIR}/test_wrapper2.sh ${ARGS_TEST_EXE} ${ARGS_TEST_ARGS}
    WORKING_DIRECTORY ${working_directory}
  )
endfunction(rift_systemtest)

