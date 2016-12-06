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
# Creation Date: 12/07/2013
# 

include(CMakeParseArguments)

set(RIFT_COVERAGE_DIR $ENV{RIFT_COVERAGE})
# Check prereqs
find_program(GCOV_PATH gcov)
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)

# Check that gcov, lcov, genhtml are installed
if(NOT GCOV_PATH)
  message(FATAL_ERROR "Error: gcov is NOT installed")
endif()

if(NOT LCOV_PATH)
  message(FATAL_ERROR "Error: lcov is NOT installed")
endif()

if(NOT GENHTML_PATH)
  message(FATAL_ERROR "Error: genhtml is NOT installed")
endif()

if(NOT CMAKE_COMPILER_IS_GNUCXX)
  message(FATAL_ERROR "Error: compiler is NOT gcc")
endif()

##
# This function creates a code coverage target
#
# rift_code_coverage(target
#                    [ EXCLUDE_FROM_COVERAGE_TARGETS ]
#                    TEST_ARGS <arguments-to-test-program>
#                    TEST_WORKING_DIRECTORY <working-directory>
#                    ENVSET_ARGS <arguments to pass to envset.sh before test execution>
#                    DEPENDS <target-dependencies>)
##
function(rift_code_coverage target)
  # Parse the function arguments
  set(parse_options EXCLUDE_FROM_COVERAGE_TARGETS)
  set(parse_onevalargs TEST_EXE TEST_WORKING_DIRECTORY)
  set(parse_multivalueargs TEST_ARGS DEPENDS EXTRA_DIRS ENVSET_ARGS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(ARGS_TEST_WORKING_DIRECTORY)
    set(working_directory ${ARGS_TEST_WORKING_DIRECTORY})
  else()
    set(working_directory ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  if(NOT ARGS_EXCLUDE_FROM_COVERAGE_TARGETS)
    set(found 0)
    foreach(coverage ${RIFT_COVERAGE_TARGET_LIST})
      if("${coverage}" MATCHES "${target}")
        set(found 1)
      endif()
    endforeach(coverage)
    if(NOT found)
      set(RIFT_COVERAGE_TARGET_LIST ${RIFT_COVERAGE_TARGET_LIST} 
        ${target} CACHE INTERNAL "Coverage Target List")
    endif()
  endif()

  if((NOT COVERAGE_BUILD MATCHES TRUE) OR (NOT CMAKE_BUILD_TYPE STREQUAL "Debug"))
    add_custom_target(${target}
      COMMAND echo 'CMAKE_BUILD_TYPE must be Debug and COVERAGE_BUILD must be TRUE'
      )

  else()
    # Use the parent directory name as component name if we're in a test/examples directory.
    # TODO: The --directory and testFilter should really be passed in explicitly.
    get_filename_component(compDir ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    if (compDir MATCHES "(.*)test(.*)" OR compDir MATCHES "(.*)example(.*)")
      get_filename_component(compDir "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
      get_filename_component(compDir "${compDir}" NAME)
      set (testFilter '${CMAKE_CURRENT_SOURCE_DIR}/*')
      set(dirs --directory ../)
    else()
      set (testFilter '${CMAKE_CURRENT_SOURCE_DIR}/test*')
      set(dirs --directory ./)
    endif()

    if (ARGS_EXTRA_DIRS)
      foreach(dir ${ARGS_EXTRA_DIRS})
        list(APPEND dirs --directory ${dir})
      endforeach()
    endif()

    set (testOutDir ${RIFT_UNITTEST_DIR}/${RIFT_SUBMODULE_PREFIX}/${compDir})

    # Create a seperate coverage directory for each coverage target.
    # This allows us to see code coverage per unit test.
    set (coverageOutDir ${RIFT_COVERAGE_DIR}/${RIFT_SUBMODULE_PREFIX}/${compDir}/${target})

    set(testArgs ${RIFT_CMAKE_BIN_DIR}/catchsegv.sh)

    # If ENVSET_ARGS was provided, add envset.sh to the command line with the envset vars
    if(ARGS_ENVSET_ARGS)
      set(testArgs ${testArgs} ${RIFT_CMAKE_BIN_DIR}/envset.sh ${ARGS_ENVSET_ARGS})
    endif(ARGS_ENVSET_ARGS)

    # Finally use the actual provided TEST_ARGS to finish building
    # the test command arguments.
    set(testArgs ${testArgs} ${ARGS_TEST_ARGS})

    # Set up the custom target for code coverage
    add_custom_target(${target}
      COMMAND mkdir -p ${testOutDir}
      COMMAND mkdir -p ${coverageOutDir}

      # Reset all execution counts to zero.
      COMMAND ${LCOV_PATH} ${dirs} --zerocounters

      # Run the test program.  Fail hard on a fatal signal (SIGSEV, etc)
      COMMAND ${RIFT_CMAKE_BIN_DIR}/test_wrapper.sh ${testOutDir} ${target} ${testArgs}

      # Capture coverage information
      COMMAND ${LCOV_PATH} --ignore-errors source ${dirs} --capture --output-file ${coverageOutDir}/${target}.info

      # Remove irrelevant information from coverage report
      COMMAND ${LCOV_PATH} --remove ${coverageOutDir}/${target}.info 'gmock/*' ${testFilter} '/usr/*' 'rwut/*'
        --output-file ${coverageOutDir}/${target}.info.cleaned

      COMMAND ${GENHTML_PATH} -o ${coverageOutDir} ${coverageOutDir}/${target}.info.cleaned
        WORKING_DIRECTORY ${working_directory}

      COMMAND ${CMAKE_COMMAND}
        -E remove ${ARGS_COVERAGE_OUTDIR}.info ${ARGS_COVERAGE_OUTDIR}.info.cleaned
      )

    if(ARGS_DEPENDS)
      add_dependencies(${target} ${ARGS_DEPENDS})
    endif()
  endif()
endfunction(rift_code_coverage)
