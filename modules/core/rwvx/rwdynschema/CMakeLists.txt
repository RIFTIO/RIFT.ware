# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 9/17/2015
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwdynschema)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

#Force -Werror
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

add_definitions(-D_GNU_SOURCE)

set(subdirs plugins)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

set(
  C_SOURCES
  src/rwdynschema.c 
  )

set(
  CXX_SOURCES
  src/rw_file_update.cc
  src/rwdynschema_load_schema.cc
  )

find_library(boost_filesystem_path NAMES boost_filesystem)
if (NOT boost_filesystem_path)
  message (FATAL_ERROR "Could not find library ${boost_filesystem_path}")
endif ()
find_library(boost_system_path NAMES boost_system)
if (NOT boost_system_path)
  message (FATAL_ERROR "Could not find library ${boost_system_path}")
endif ()

add_library(
  rwdynschema
  SHARED
    ${C_SOURCES}
    ${auto_c_files}
    ${CXX_SOURCES}
  )

include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdtsperf/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwschema/src
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwdts/plugins/proto
  )

target_link_libraries(
  rwdynschema
  PRIVATE
    ${boost_filesystem_path}
    ${boost_system_path}
    CoreFoundation
    crypto
    dispatch
    glib-2.0
    gobject-2.0
    protobuf-c
    pthread
    rt
    rwdts_api
    rwdynschema_yang_gen
    rwgeneric_yang_gen
    rwlib
    rwlog
    rwmemlog
    rwsched
    rwschema_yang_gen
    rwyangutil_lib
    ssl
  )

add_dependencies(
  rwdynschema
  rwsched
  rwlog
  rwdts_api
  rwlogger_yang.headers
  rwgeneric_yang.headers
  rwschema_yang.headers
  rwlog_event_auto
  rwyangutil_lib
  process_rwlog_evt_files
  )

##
# Install the header files and targets
##
set(INSTALL_HFILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdynschema/include/riftware/rwdynschema.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdynschema/include/riftware/rw_file_update.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdynschema/include/riftware/rwdynschema_load_schema.h  
  )

install(
  FILES
    ${INSTALL_HFILES}
  DESTINATION
    usr/include
  COMPONENT
    ${PKG_LONG_NAME}
  )
install(
  TARGETS
    rwdynschema
  LIBRARY
  DESTINATION
    usr/lib
  COMPONENT
    ${PKG_LONG_NAME}
  )

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME
    "RW.DynSchema QuickStart and API Reference"
  DOXY_FILES 
    include/riftware/rwdynschema.h 
    include/riftware/rw_file_update.h
    include/riftware/rwdynschema_load_schema.h
  DEST_DIR
    ${PKG_NAME}
  )

##
# Add the cmake magic to generate the GI bindings
# Note that the g-ir-scanner scans the annotations
# in HFILES and generates the gir and typelib files
##
set(GI_NAME rwdynschema)
set(GI_VERSION 1.0)
set(GI_NAMESPACE RwDynSchema)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(${GI_NAME}_gi
  NAMESPACE
    ${GI_NAMESPACE}
  VERSION
    ${GI_VERSION}
  VAPI_PREFIX
    ${UNCAMEL_GI_NAMESPACE}
  GIR_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/rwtasklet
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/vala
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwdts
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwschema/yang
  LIBRARY_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwdts
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwdynschema
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
  PACKAGES
    CF-1.0
    RwDts-1.0
    RwSched-1.0
    RwTypes-1.0
    RwTasklet-1.0
    RwYang-1.0
    RwKeyspec-1.0
    RwLog-1.0
    RwManifestYang-1.0
    ProtobufC-1.0
  HFILES
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdynschema/include/riftware/rwdynschema.h
  LIBRARIES
    rwdynschema 
  IDENTIFIER_FILTER_CMD
    "python ${CMAKE_CURRENT_SOURCE_DIR}/src/rwdynschema_gi_filter.py"
  IDENTIFIER_PREFIX
    "RwDynSchema"
  GI_INCLUDE_HFILES 
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdynschema/include/riftware/rwdynschema.h
  INCLUDE_PATHS
    ${CMAKE_INSTALL_PREFIX}/usr/include/dispatch
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdynschema/include/
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwschema/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
  DEPENDS
    rwdynschema
    rwdts_gi
    rwdts_api
    rwschema_yang
    rwmanifest_yang
  GENERATE_GI_OVERRIDE
  )

##
# Install the generated gir and typelib files
##
rift_install_vala_artifacts(
  COMPONENT
    ${PKG_LONG_NAME}
  GIR_FILES
    ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES
    ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES
    ${UNCAMEL_GI_NAMESPACE}-${GI_VERSION}.vapi
  LUA_OVERRIDES
    ${GI_NAMESPACE}.lua
  PYTHON_OVERRIDES
    ${GI_NAMESPACE}.py
  DEST_PREFIX
    .
  )



## testing
set(test_srcs test/rwdynschema_test.cpp)
set(test_libs
  rwdynschema
  rwyang
  ${boost_filesystem_path}
  ${boost_system_path}
  )

rift_gtest(rwdynschema_gtest
  TEST_SRCS
    ${test_srcs}
  TEST_LIBS
    ${test_libs}
  TEST_ARGS
    --gtest_shuffle
    --gtest_repeat=1
  )


