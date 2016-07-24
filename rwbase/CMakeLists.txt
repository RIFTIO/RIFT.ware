
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules")

set(RIFT_NONSUBMODULE_PROJECT TRUE)
include(rift_submodule)

rift_add_subdirs(SUBDIR_LIST python include)

rift_add_submodule_targets(SUBMODULE_PACKAGE_NAME "rwbase")


function(__gen_version)
  # Generate the riftware version header
  find_package(Git)

  file(WRITE ${CMAKE_BINARY_DIR}/riftware-version.h.in
    "\#define RIFTWARE_VERSION \"@VERSION@\"\n"
    "\#define RIFTWARE_GIT \"@GIT_SHA@\"\n"
    "\#define RIFTWARE_DATE \"@DATE@\"\n"
  )


  execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/packaging/getbuild")

  file(WRITE ${CMAKE_BINARY_DIR}/riftware-version.cmake
    "execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
        COMMAND date -u
        OUTPUT_VARIABLE DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
        COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/../.build/VERSION 
        OUTPUT_VARIABLE VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    configure_file(\${SRC} \${DST} @ONLY)
  ")

  add_custom_target(
    riftware-version ALL
    ${CMAKE_COMMAND} -D SRC=${CMAKE_BINARY_DIR}/riftware-version.h.in
        -D DST=${CMAKE_BINARY_DIR}/include/riftware-version.h
        -P ${CMAKE_BINARY_DIR}/riftware-version.cmake
  )

  install(FILES ${CMAKE_BINARY_DIR}/include/riftware-version.h
      DESTINATION usr/include/riftware
      COMPONENT riftware)

endfunction()
__gen_version()


