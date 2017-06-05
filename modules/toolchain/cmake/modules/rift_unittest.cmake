# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu / Andrew Golovanov
# Creation Date: 12/07/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(CMakeParseArguments)

set(RIFT_UNITTEST_DIR $ENV{RIFT_UNIT_TEST})

##
# This function creates a unittest target
#
# rift_unittest(target
#               [ EXCLUDE_FROM_UNITTEST_TARGETS ]
#               [ LONG_UNITTEST_TARGET ]
#               TEST_ARGS <command line arguments for test program>
#               ENVSET_ARGS <arguments to pass to envset.sh before test execution>
#               TEST_WORKING_DIRECTORY <working-directory>
#               DEPENDS <target-dependencies>)
##
function(rift_unittest target)
  # Parse the function arguments
  set(parse_options ADD_TO_COVERAGE_TARGETS EXCLUDE_FROM_UNITTEST_TARGETS LONG_UNITTEST_TARGET)
  set(parse_onevalargs TEST_WORKING_DIRECTORY)
  set(parse_multivalueargs TEST_ARGS DEPENDS ENVSET_ARGS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  # Since Jenkins/CI builds only either run rw.unittest or rw.coverage, we need
  # a way of adding Python unittests to the coverage target without going
  # through the whole gcov path.  This following block was the most expedient
  # solution, but is duplicated from rift_code_coverage.cmake.
  if(ARGS_ADD_TO_COVERAGE_TARGETS)
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

  if(NOT ARGS_EXCLUDE_FROM_UNITTEST_TARGETS)
    set(found 0)
    if(ARGS_LONG_UNITTEST_TARGET)
      foreach(unit ${RIFT_LONG_UNITTEST_TARGET_LIST})
        if("${unit}" MATCHES "${target}")
          set(found 1)
        endif()
      endforeach(unit)
      if(NOT found)
        set(RIFT_LONG_UNITTEST_TARGET_LIST ${RIFT_LONG_UNITTEST_TARGET_LIST}
          ${target} CACHE INTERNAL "Long Unittest Target List")
      endif()
    else()
      foreach(unit ${RIFT_QUICK_UNITTEST_TARGET_LIST})
        if("${unit}" MATCHES "${target}")
          set(found 1)
        endif()
      endforeach(unit)
      if(NOT found)
        set(RIFT_QUICK_UNITTEST_TARGET_LIST ${RIFT_QUICK_UNITTEST_TARGET_LIST}
          ${target} CACHE INTERNAL "Unittest Target List")
      endif()
    endif()
  endif()

  if(ARGS_TEST_WORKING_DIRECTORY)
    set(working_directory ${ARGS_TEST_WORKING_DIRECTORY})
  else()
    set(working_directory ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  set(testArgs ${RIFT_CMAKE_BIN_DIR}/catchsegv.sh)

  # If ENVSET_ARGS was provided, add envset.sh to the command line with the envset vars
  if(ARGS_ENVSET_ARGS)
    set(testArgs ${testArgs} ${RIFT_CMAKE_BIN_DIR}/envset.sh ${ARGS_ENVSET_ARGS})
  endif(ARGS_ENVSET_ARGS)

  # Finally use the actual provided TEST_ARGS to finish building
  # the test command arguments.
  set(testArgs ${testArgs} ${ARGS_TEST_ARGS})

  # Set up the custom target for the unittest
  set(outputDir ${RIFT_UNITTEST_DIR}/${RIFT_SUBMODULE_PREFIX}/${compDir})
  add_custom_target(${target}
    COMMAND mkdir -p ${outputDir}
    COMMAND ${RIFT_CMAKE_BIN_DIR}/test_wrapper.sh ${outputDir} ${target} ${testArgs}
    WORKING_DIRECTORY ${working_directory}
  )

  # Add any dependencies
  if(ARGS_DEPENDS)
    add_dependencies(${target} ${ARGS_DEPENDS})
  endif()
endfunction(rift_unittest)


##
# This function in general case builds a unittest executable target.
#      <target>              - Test executable
#
# Mandatory arguments:
#
#      target_prefix         - a name of the test target prefix 
#      TEST_SRCS             - source C/C++ files that constitute the unittest
#                              target (don't include GMock files here)
#      TEST_LIBS             - libraries that have to be linked to the unittest
#                              target (don't include GMock libraries here)
#      DEPENDS             - unittest dependencies
#                              (don't include GMock dependencies here)
#
# You can add a call to custom initialization function for the entire test suite
# by passing the ADD_SETUP.
##
function(rift_unittest_build target)
  # Parse the function arguments set(parse_options ADD_SETUP) set(parse_onevalargs)
  set(parse_multivalueargs TEST_SRCS TEST_LIBS DEPENDS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(GMCK gmock-${GMOCK_VERSION})

  add_executable (${target} ${ARGS_TEST_SRCS})
  set (compFlags -DRWUT_TEST)
  if(ARGS_ADD_SETUP)
    set (compFlags "${compFlags} -DTEST_SETUP")
    set (gmockLib gmock)
  else()
    set (gmockLib gmock_main)
  endif()

  set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${compFlags})
  target_link_libraries(${target} ${ARGS_TEST_LIBS} ${gmockLib} pthread)
  add_dependencies (${target} ${ARGS_DEPENDS} ${GMCK})
endfunction(rift_unittest_build)


##
# Function to combine the build, unittest, and coverage targets of Google Framework Unit Tests

##
# This function creates a unittest target
#
# rift_gtest(test_name
#            [ EXCLUDE_FROM_UNITTEST_TARGETS ]
#            [ LONG_UNITTEST_TARGET ]
#            TEST_ARGS <arguments-to-test-program>
#            TEST_SRCS <sources to comprise unit test>
#            TEST_LIBS <libraries that have to be linked to the unittest>
#            ENVSET_ARGS <arguments to pass to envset.sh before test execution>
#            DEPENDS <unittest dependencies>)
#
# You can add a call to custom initialization function for the entire test suite
# by passing the ADD_SETUP.
##
function(rift_gtest target)
  set (parse_options ADD_SETUP EXCLUDE_FROM_COVERAGE_TARGETS EXCLUDE_FROM_UNITTEST_TARGETS LONG_UNITTEST_TARGET)

  set (parse_onevalargs)
  set (parse_multivalueargs TEST_SRCS TEST_LIBS DEPENDS TEST_ARGS ENVSET_ARGS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  get_filename_component(curDir "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
  rift_submodule_relative_source_path(relSubDir ${curDir})

  set (gTestOutputArg
    --gtest_output=xml:${RIFT_UNITTEST_DIR}/${RIFT_SUBMODULE_PREFIX}/${relSubDir}/${target}.xml)
  set (testExe ${CMAKE_CURRENT_BINARY_DIR}/${target})

  # First is the gtest executable
  set (testArgs ${testExe})
  # Next is the gtest output parameter
  set (testArgs ${testArgs} ${gTestOutputArg})
  # Lastly, the user-defined executable arguments
  set (testArgs ${testArgs} ${ARGS_TEST_ARGS})

  # If the options were passed in, then we need to forward them to the correct functions
  if (ARGS_ADD_SETUP)
    set(ADD_SETUP ADD_SETUP)
  endif()
  if (ARGS_EXCLUDE_FROM_UNITTEST_TARGETS)
    set(EXCLUDE_FROM_UNITTEST_TARGETS EXCLUDE_FROM_UNITTEST_TARGETS)
  endif()
  if (ARGS_LONG_UNITTEST_TARGET)
    set(LONG_UNITTEST_TARGET LONG_UNITTEST_TARGET)
  endif()
  if (ARGS_EXCLUDE_FROM_COVERAGE_TARGETS)
    set(EXCLUDE_FROM_COVERAGE_TARGETS EXCLUDE_FROM_COVERAGE_TARGETS)
  endif()

  # Add a target to build the unit test
  rift_unittest_build(${target}
    TEST_SRCS ${ARGS_TEST_SRCS}
    TEST_LIBS ${ARGS_TEST_LIBS}
    DEPENDS ${ARGS_DEPENDS}
    ${ADD_SETUP}
  )

  # Add a target to run the unit test on "make rw.unittest"
  rift_unittest(${target}_ut
    TEST_ARGS ${testArgs}
    ENVSET_ARGS ${ARGS_ENVSET_ARGS}
    DEPENDS ${target}
    ${LONG_UNITTEST_TARGET}
    ${EXCLUDE_FROM_UNITTEST_TARGETS}
  )

  # Add a target to run the unit test on "make rw.coverage"
  rift_code_coverage(${target}_cov
    TEST_ARGS ${testArgs}
    ENVSET_ARGS ${ARGS_ENVSET_ARGS}
    DEPENDS ${target}
    ${EXCLUDE_FROM_COVERAGE_TARGETS}
  )

endfunction(rift_gtest)

##
# This function adds python unittests to either the rw.unittest (and optionally
# the rw.unittest_long) target.
#
# rift_pytest(target
#             [ EXCLUDE_FROM_UNITTEST_TARGETS ]
#             [ LONG_UNITTEST_TARGET ]
#             TEST_ARGS <arguments-to-test-program>
#             TEST_WORKING_DIRECTORY <working-directory>
#             ENVSET_ARGS <arguments to pass to envset.sh before test execution>
#             DEPENDS <target-dependencies>)
##
function(rift_pytest target)
  # Parse the function arguments
  set(parse_options EXCLUDE_FROM_UNITTEST_TARGETS LONG_UNITTEST_TARGET)
  set(parse_onevalargs TEST_WORKING_DIRECTORY)
  set(parse_multivalueargs TEST_ARGS DEPENDS ENVSET_ARGS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  # Add the target to run the unit test on "make rw.unittest"
  rift_unittest(${target}
    TEST_ARGS /usr/bin/python ${ARGS_TEST_ARGS}
    TEST_WORKING_DIRECTORY ${ARGS_TEST_WORKING_DIRECTORY}
    ENVSET_ARGS ${ARGS_ENVSET_ARGS}
    DEPENDS ${ARGS_DEPENDS}
    ADD_TO_COVERAGE_TARGETS
    ${ARGS_LONG_UNITTEST_TARGET}
    ${ARGS_EXCLUDE_FROM_UNITTEST_TARGETS}
  )

endfunction(rift_pytest)

##
# This function adds python unittests to either the rw.unittest (and optionally
# the rw.unittest_long) target.
#
# rift_py3test(target
#             [ EXCLUDE_FROM_UNITTEST_TARGETS ]
#             [ LONG_UNITTEST_TARGET ]
#             TEST_ARGS <arguments-to-test-program>
#             TEST_WORKING_DIRECTORY <working-directory>
#             ENVSET_ARGS <arguments to pass to envset.sh before test execution>
#             DEPENDS <target-dependencies>)
##
function(rift_py3test target)
  # Parse the function arguments
  set(parse_options EXCLUDE_FROM_UNITTEST_TARGETS LONG_UNITTEST_TARGET)
  set(parse_onevalargs TEST_WORKING_DIRECTORY)
  set(parse_multivalueargs TEST_ARGS DEPENDS ENVSET_ARGS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})
  if (ARGS_EXCLUDE_FROM_UNITTEST_TARGETS)
    set(EXCLUDE_FROM_UNITTEST_TARGETS EXCLUDE_FROM_UNITTEST_TARGETS)
  endif()
  if (ARGS_LONG_UNITTEST_TARGET)
    set(LONG_UNITTEST_TARGET LONG_UNITTEST_TARGET)
  endif()
  if (ARGS_EXCLUDE_FROM_COVERAGE_TARGETS)
    set(EXCLUDE_FROM_COVERAGE_TARGETS EXCLUDE_FROM_COVERAGE_TARGETS)
  endif()


  # Add the target to run the unit test on "make rw.unittest"
  rift_unittest(${target}
    TEST_ARGS /usr/bin/python3 ${ARGS_TEST_ARGS}
    TEST_WORKING_DIRECTORY ${ARGS_TEST_WORKING_DIRECTORY}
    ENVSET_ARGS ${ARGS_ENVSET_ARGS}
    DEPENDS ${ARGS_DEPENDS}
    ADD_TO_COVERAGE_TARGETS
    ${ARGS_LONG_UNITTEST_TARGET}
    ${ARGS_EXCLUDE_FROM_UNITTEST_TARGETS}
  )
endfunction(rift_py3test)


##
# This function adds python unittests to either the rw.unittest (and optionally
# the rw.unittest_long) target.
#
# rift_luatest(target
#              [ EXCLUDE_FROM_UNITTEST_TARGETS ]
#              [ LONG_UNITTEST_TARGET ]
#              TEST_ARGS <arguments-to-test-program>
#              TEST_WORKING_DIRECTORY <working-directory>
#              ENVSET_ARGS <arguments to pass to envset.sh before test execution>
#              DEPENDS <target-dependencies>)
##
function(rift_luatest target)
  # Parse the function arguments
  set(parse_options EXCLUDE_FROM_UNITTEST_TARGETS LONG_UNITTEST_TARGET)
  set(parse_onevalargs TEST_WORKING_DIRECTORY)
  set(parse_multivalueargs TEST_ARGS DEPENDS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  # Add the target to run the unit test on "make rw.unittest"
  rift_unittest(${target}
    TEST_ARGS /usr/bin/luajit ${ARGS_TEST_ARGS}
    TEST_WORKING_DIRECTORY ${ARGS_TEST_WORKING_DIRECTORY}
    ENVSET_ARGS ${ARGS_ENVSET_ARGS}
    DEPENDS ${ARGS_DEPENDS}
    ADD_TO_COVERAGE_TARGETS
    ${ARGS_LONG_UNITTEST_TARGET}
    ${ARGS_EXCLUDE_FROM_UNITTEST_TARGETS}
  )

endfunction(rift_luatest)
