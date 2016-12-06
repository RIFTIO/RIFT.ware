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
# Author(s): Tim Mortsolf
# Creation Date: 8/24/2013
# 

##
# Allow specific compiler warnings
##
rift_allow_compiler_warning(unused-but-set-variable)

##
# rw_tasklet_plugin-1.0
#
# Set some RIFT specific VALA variables for this cmake project
##
set(VALA_NAME rw_tasklet_plugin)
set(VALA_FILES ${VALA_NAME}.vala)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
rift_camel(${VALA_NAME} VALA_NAMESPACE)
set(VALA_TYPELIB_PREFIX ${VALA_NAMESPACE}-${VALA_VERSION})

##
# rw_tasklet_plugin-1.0
#
# This function compiles all the vala sources
# Generates the package .h/.c sources, .gir file, .vapi file, and the .so
##
rift_add_vala(
  ${VALA_LONG_NAME}
  VALA_FILES ${VALA_FILES}
  VALA_PACKAGES
    rw_sched-1.0 
    cf-1.0 
    rw_types-1.0
    rw_log-1.0 
    rw_trace-1.0 
    rw_tasklet-1.0 
    protobuf_c-1.0 
    rw_yang_pb-1.0 
    rw_yang-1.0 
    rw_schema_proto-1.0 
    rw_keyspec-1.0
    rw_manifest_yang-1.0
  VAPI_DIRS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/rwtasklet
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
  GIR_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/rwtasklet
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwschema/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/plugins/yang
  GENERATE_HEADER_FILE ${VALA_NAME}.h
  GENERATE_SO_FILE lib${VALA_LONG_NAME}.so
  GENERATE_VAPI_FILE ${VALA_LONG_NAME}.vapi
  GENERATE_GIR_FILE ${VALA_TYPELIB_PREFIX}.gir
  GENERATE_TYPELIB_FILE ${VALA_TYPELIB_PREFIX}.typelib
  LIBRARIES rwsched rwlog rwtrace
  DEPENDS rwexec-1.0 rwsched_gi rwlog_gi rwtasklet_gi rwvcs
  )

##
# rw_tasklet_plugin-1.0
#
# This function creates an install target for artifacts generated from the vala files
##
rift_install_vala_artifacts(
  HEADER_FILES ${VALA_NAME}.h
  SO_FILES lib${VALA_LONG_NAME}.so
  VAPI_FILES ${VALA_LONG_NAME}.vapi
  GIR_FILES ${VALA_TYPELIB_PREFIX}.gir
  TYPELIB_FILES ${VALA_TYPELIB_PREFIX}.typelib
  COMPONENT ${INSTALL_COMPONENT}
  DEST_PREFIX .
  )

##
# rwvcs_zk-1.0 
set(VALA_NAME rwvcs_zk)
set(VALA_FILES ${VALA_NAME}.vala)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
set(VALA_TYPELIB_PREFIX RwVcsZk-${VALA_VERSION})

rift_add_vala(
  ${VALA_LONG_NAME}
  VALA_FILES ${VALA_FILES}
  VALA_PACKAGES rw_types-1.0 rw_log-1.0
  VAPI_DIRS ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
  GIR_PATHS ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
  GENERATE_HEADER_FILE ${VALA_NAME}.h
  GENERATE_SO_FILE lib${VALA_LONG_NAME}.so
  GENERATE_VAPI_FILE ${VALA_LONG_NAME}.vapi
  GENERATE_GIR_FILE ${VALA_TYPELIB_PREFIX}.gir
  GENERATE_TYPELIB_FILE ${VALA_TYPELIB_PREFIX}.typelib
  DEPENDS rwlog_gi
  )

rift_install_vala_artifacts(
  HEADER_FILES ${VALA_NAME}.h
  SO_FILES lib${VALA_LONG_NAME}.so
  VAPI_FILES ${VALA_LONG_NAME}.vapi
  GIR_FILES ${VALA_TYPELIB_PREFIX}.gir
  TYPELIB_FILES ${VALA_TYPELIB_PREFIX}.typelib
  COMPONENT ${INSTALL_COMPONENT}
  DEST_PREFIX .
  )


set(subdirs
  rwvcs_zk
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})
