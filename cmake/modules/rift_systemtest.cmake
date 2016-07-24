# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
    COMMAND ${PROJECT_TOP_DIR}/scripts/env/test_wrapper2.sh ${ARGS_TEST_EXE} ${ARGS_TEST_ARGS}
    WORKING_DIRECTORY ${working_directory}
  )
endfunction(rift_systemtest)

