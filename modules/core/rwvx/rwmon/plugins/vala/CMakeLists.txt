# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Joshua Downer
# Creation Date: 2015/10/27
# 

##
# Allow specific compiler warnings
##
rift_allow_compiler_warning(unused-but-set-variable)

set(VALA_NAME rwmon)
set(VALA_FILES ${VALA_NAME}.vala)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
set(VALA_TYPELIB_PREFIX RwMon-${VALA_VERSION})

rift_add_vala(
  ${VALA_LONG_NAME}
  VALA_FILES ${VALA_FILES}
  VALA_PACKAGES
    rw_types-1.0 rw_yang-1.0 rw_keyspec-1.0 rw_yang_pb-1.0 rw_schema_proto-1.0
    rw_log_yang-1.0 rw_base_yang-1.0 rwmon_yang-1.0 rw_manifest_yang-1.0 protobuf_c-1.0 ietf_netconf_yang-1.0
    rw_log-1.0 rwcal_yang-1.0
  VAPI_DIRS ${RIFT_SUBMODULE_BINARY_ROOT}/rwmon/plugins/yang
            ${RIFT_SUBMODULE_BINARY_ROOT}/rwcal/plugins/yang
            ${RIFT_SUBMODULE_BINARY_ROOT}/rwschema/yang
            ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
            ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
  GIR_PATHS ${RIFT_SUBMODULE_BINARY_ROOT}/rwmon/plugins/yang
            ${RIFT_SUBMODULE_BINARY_ROOT}/rwcal/plugins/yang
            ${RIFT_SUBMODULE_BINARY_ROOT}/rwschema/yang
            ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
            ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
  GENERATE_HEADER_FILE${VALA_NAME}.h
  GENERATE_SO_FILE lib${VALA_LONG_NAME}.so
  GENERATE_VAPI_FILE ${VALA_LONG_NAME}.vapi
  GENERATE_GIR_FILE ${VALA_TYPELIB_PREFIX}.gir
  GENERATE_TYPELIB_FILE ${VALA_TYPELIB_PREFIX}.typelib
  DEPENDS rwmon_yang rwcal_yang rwlog_gi rwschema_yang
  )

rift_install_vala_artifacts(
  SO_FILES lib${VALA_LONG_NAME}.so
  VAPI_FILES ${VALA_LONG_NAME}.vapi
  GIR_FILES ${VALA_TYPELIB_PREFIX}.gir
  TYPELIB_FILES ${VALA_TYPELIB_PREFIX}.typelib
  COMPONENT ${PKG_LONG_NAME}
  DEST_PREFIX .
  )

set(subdirs
  rwmon_ceilometer
  rwmon_mock
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})
